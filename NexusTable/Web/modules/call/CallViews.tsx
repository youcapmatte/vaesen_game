"use client";

import { ActionIcon, Box, Button, Group, Text, Tooltip } from "@mantine/core";
import { useLocalParticipant, useTranscriptions } from "@livekit/components-react";
import { IconMicrophone, IconMicrophoneOff, IconPhone, IconPhoneOff, IconVideo, IconVideoOff } from "@tabler/icons-react";
import { useMemo } from "react";
import type { SocietyMember } from "@/common/api/society";
import { ErrorNote } from "@/common/client/components/PageFrame";
import { useCall, useGamemaster } from "./Call";

/** Join or leave the call, and turn your own camera and microphone on and off. */
export function CallBar() {
  const { status, error, join, leave } = useCall();
  const { localParticipant, isMicrophoneEnabled, isCameraEnabled } = useLocalParticipant();

  if (status !== "joined") {
    return (
      <Box className="nx-panel" p="xs">
        <Button
          fullWidth
          size="sm"
          color="blood"
          leftSection={<IconPhone size={16} />}
          loading={status === "joining"}
          disabled={status === "unavailable"}
          onClick={() => void join()}
        >
          Join the call
        </Button>
        <ErrorNote message={error} />
      </Box>
    );
  }

  return (
    <Box className="nx-panel" p="xs">
      <Group gap={6} justify="space-between" wrap="nowrap">
        <Group gap={4}>
          <Tooltip label={isMicrophoneEnabled ? "Mute" : "Unmute"}>
            <ActionIcon variant={isMicrophoneEnabled ? "light" : "filled"} color={isMicrophoneEnabled ? "gold" : "blood"} onClick={() => void localParticipant.setMicrophoneEnabled(!isMicrophoneEnabled)} aria-label="Microphone">
              {isMicrophoneEnabled ? <IconMicrophone size={16} /> : <IconMicrophoneOff size={16} />}
            </ActionIcon>
          </Tooltip>
          <Tooltip label={isCameraEnabled ? "Camera off" : "Camera on"}>
            <ActionIcon variant={isCameraEnabled ? "light" : "filled"} color={isCameraEnabled ? "gold" : "umber.6"} onClick={() => void localParticipant.setCameraEnabled(!isCameraEnabled)} aria-label="Camera">
              {isCameraEnabled ? <IconVideo size={16} /> : <IconVideoOff size={16} />}
            </ActionIcon>
          </Tooltip>
        </Group>
        <Button size="compact-sm" variant="subtle" color="blood.4" leftSection={<IconPhoneOff size={14} />} onClick={leave}>Leave</Button>
      </Group>
      <ErrorNote message={error} />
    </Box>
  );
}

export interface TranscriptLine {
  id: string;
  at: number;
  /** The Gamemaster, or the player who spoke to it. */
  fromGm: boolean;
  speaker: string;
  text: string;
}

/**
 * What was said to and by the Gamemaster, from the agent's transcriptions (LiveKit text streams). Players' table
 * talk is never transcribed: the agent only hears the push-to-talk turns.
 */
export function useTableTranscript(members: SocietyMember[]): TranscriptLine[] {
  const streams = useTranscriptions();
  const gm = useGamemaster();
  return useMemo(() => streams
    .filter((s) => s.text.trim())
    .map((s) => {
      const fromGm = s.participantInfo.identity === gm.participant?.identity;
      const member = members.find((m) => m.accountId === s.participantInfo.identity);
      return {
        id: s.streamInfo.id,
        at: s.streamInfo.timestamp,
        fromGm,
        speaker: fromGm ? "The Gamemaster" : member?.character?.name ?? member?.displayName ?? "Someone",
        text: s.text.trim(),
      };
    }), [streams, gm.participant, members]);
}

/** One line of the conversation with the Gamemaster in the log. */
export function GmLine({ line }: { line: TranscriptLine }) {
  return (
    <Box py={8} style={{ borderBottom: "1px solid rgba(184,149,90,0.15)" }}>
      <Group justify="space-between" gap={4} wrap="nowrap">
        <Text fz="sm" className="nx-display" c={line.fromGm ? "var(--nx-gold)" : "var(--nx-parchment)"}>
          {line.speaker}{line.fromGm ? "" : " · to the Gamemaster"}
        </Text>
        <Text fz={11} c="var(--nx-sepia)">{new Date(line.at).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}</Text>
      </Group>
      <Text fz="sm" c={line.fromGm ? "var(--nx-parchment)" : "var(--nx-parchment-2)"} className={line.fromGm ? "nx-flourish" : undefined} style={{ whiteSpace: "pre-wrap" }}>
        {line.text}
      </Text>
    </Box>
  );
}
