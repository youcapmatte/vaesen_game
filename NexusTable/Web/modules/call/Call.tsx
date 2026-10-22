"use client";

import { isTrackReference, LiveKitRoom, RoomAudioRenderer, type TrackReference, useLocalParticipant, useParticipantAttributes, useRemoteParticipants, useRoomContext, useTracks } from "@livekit/components-react";
import { type Participant, ParticipantKind, type RemoteParticipant, Track } from "livekit-client";
import { createContext, useCallback, useContext, useMemo, useState } from "react";
import { CallApi, type CallJoin, type GmControl, GmProtocol } from "@/common/api/call";
import { Http, useApi } from "@/common/client/core/Api";

type CallStatus = "idle" | "joining" | "joined" | "unavailable";

interface CallState {
  status: CallStatus;
  error?: string;
  join: () => Promise<void>;
  leave: () => void;
}

const CallContext = createContext<CallState>({ status: "idle", join: async () => {}, leave: () => {} });

/**
 * The LiveKit room around the table. Joining needs a click (browsers only play and capture audio after one), then
 * everyone hears and sees everyone. Before joining, the room exists but isn't connected, so the hooks below still work.
 */
export function CallProvider({ children }: { children: React.ReactNode }) {
  const call = useApi();
  const [join, setJoin] = useState<CallJoin>();
  const [status, setStatus] = useState<CallStatus>("idle");
  const [error, setError] = useState<string>();

  const start = useCallback(async () => {
    setStatus("joining");
    const result = await call<CallJoin>(Http.POST, CallApi.join);
    if (!result.success || !result.result) {
      setStatus(result.errorCode === "call_not_configured" ? "unavailable" : "idle");
      setError(result.errorMessage);
      return;
    }
    setError(undefined);
    setJoin(result.result);
  }, [call]);

  const leave = useCallback(() => {
    setJoin(undefined);
    setStatus("idle");
  }, []);

  const value = useMemo(() => ({ status, error, join: start, leave }), [status, error, start, leave]);

  return (
    <CallContext.Provider value={value}>
      <LiveKitRoom
        serverUrl={join?.url}
        token={join?.token}
        connect={Boolean(join)}
        audio
        video
        onConnected={() => setStatus("joined")}
        onDisconnected={() => {
          setJoin(undefined);
          setStatus("idle");
        }}
        onError={(e) => setError(e.message)}
        onMediaDeviceFailure={() => setError("No camera or microphone was allowed. You can still listen.")}
        style={{ display: "contents" }}
      >
        <RoomAudioRenderer />
        {children}
      </LiveKitRoom>
    </CallContext.Provider>
  );
}

export function useCall() {
  return useContext(CallContext);
}

export interface SeatMedia {
  participant: Participant;
  camera?: TrackReference;
  micOn: boolean;
  speaking: boolean;
}

/** Each connected person's camera and microphone, by account id (the LiveKit identity). */
export function useSeatMedia(): Map<string, SeatMedia> {
  const cameras = useTracks([Track.Source.Camera], { onlySubscribed: false });
  const remotes = useRemoteParticipants();
  const { localParticipant } = useLocalParticipant();

  return useMemo(() => {
    const media = new Map<string, SeatMedia>();
    const people: Participant[] = [localParticipant, ...remotes.filter((p) => p.kind !== ParticipantKind.AGENT)];
    for (const participant of people) {
      if (!participant.identity) continue;
      const camera = cameras.filter(isTrackReference).find((t) => t.participant.identity === participant.identity && !t.publication.isMuted);
      media.set(participant.identity, {
        participant,
        camera,
        micOn: participant.isMicrophoneEnabled,
        speaking: participant.isSpeaking,
      });
    }
    return media;
  }, [cameras, remotes, localParticipant]);
}

export interface GamemasterState {
  participant?: RemoteParticipant;
  state?: string;
  latencyMs?: number;
  speaking: boolean;
}

/** The GM agent in the room, if it is there: what it is doing, and how fast it answered last. */
export function useGamemaster(): GamemasterState {
  const agent = useRemoteParticipants().find((p) => p.kind === ParticipantKind.AGENT);
  const { attributes } = useParticipantAttributes({ participant: agent });
  return {
    participant: agent,
    state: attributes?.[GmProtocol.stateAttribute],
    latencyMs: attributes?.[GmProtocol.latencyAttribute] ? Number(attributes[GmProtocol.latencyAttribute]) : undefined,
    speaking: agent?.isSpeaking ?? false,
  };
}

/** Talking to the Gamemaster, the host's controls, and telling it about rolls. */
export function useGamemasterActions() {
  const room = useRoomContext();
  const { participant: agent } = useGamemaster();

  const rpc = useCallback(async (method: string, payload = "") => {
    if (!agent) return undefined;
    try {
      return await room.localParticipant.performRpc({ destinationIdentity: agent.identity, method, payload, responseTimeout: 10_000 });
    } catch {
      return undefined;
    }
  }, [room, agent]);

  return {
    present: Boolean(agent),
    startTurn: () => rpc(GmProtocol.startTurn),
    endTurn: () => rpc(GmProtocol.endTurn),
    control: (action: GmControl) => rpc(GmProtocol.control, JSON.stringify({ action })),
    /** The agent reads the roll back from the API itself, so only the id travels. */
    reportRoll: async (rollId: string) => {
      if (!agent) return;
      await room.localParticipant.publishData(new TextEncoder().encode(JSON.stringify({ rollId })), {
        reliable: true,
        topic: GmProtocol.rollTopic,
        destinationIdentities: [agent.identity],
      });
    },
  };
}
