"use client";

import { Avatar, Badge, Box, Button, Group, Menu, Stack, Text, Tooltip, UnstyledButton } from "@mantine/core";
import { VideoTrack } from "@livekit/components-react";
import { IconArrowForwardUp, IconHourglass, IconMicrophone, IconMicrophoneOff, IconPlayerPause, IconPlayerPlay, IconRepeat, IconScript, IconVideoOff, IconWaveSine, IconX } from "@tabler/icons-react";
import { useState } from "react";
import { CallApi } from "@/common/api/call";
import type { SocietyMember } from "@/common/api/society";
import { Http, useApi } from "@/common/client/core/Api";
import { ErrorNote } from "@/common/client/components/PageFrame";
import { SightMark } from "@/common/client/components/Ornament";
import type { PresenceEntry } from "@/common/client/signalr/useTable";
import { type SeatMedia, useGamemaster, useGamemasterActions } from "@/modules/call/Call";
import styles from "./Table.module.css";

const SEATS = 4;

/**
 * The camera tiles down the side: one per member, missing ones shown as empty chairs, then the Gamemaster
 * (audio only). Video and speaking come from the call; who is at the table and who holds push-to-talk from the hub.
 */
export function Seats({ members, presence, media, archetypeName, isHost, onOpenSheet }: {
  members: SocietyMember[];
  presence: PresenceEntry[];
  media: Map<string, SeatMedia>;
  archetypeName: (id?: string) => string;
  isHost: boolean;
  onOpenSheet: (characterId: string) => void;
}) {
  const here = new Map(presence.map((p) => [p.accountId, p]));
  const seated = [...members].sort((a, b) => Number(here.has(b.accountId)) - Number(here.has(a.accountId))).slice(0, SEATS);

  return (
    <Stack gap="sm" className={`${styles.seats} nx-scroll`}>
      {seated.map((member) => {
        const presenceEntry = here.get(member.accountId);
        const seat = media.get(member.accountId);
        const connected = Boolean(presenceEntry) || Boolean(seat);
        const talking = presenceEntry?.talkingToGm ?? false;
        const character = member.character;
        return (
          <UnstyledButton
            key={member.accountId}
            className={`${styles.tile} ${talking ? styles.talking : seat?.speaking ? styles.speaking : ""} ${connected ? "" : styles.missing}`}
            onClick={() => character && onOpenSheet(character.id)}
            disabled={!character}
          >
            <Box className={styles.video}>
              {seat?.camera ? (
                <VideoTrack trackRef={seat.camera} className={styles.videoTrack} />
              ) : (
                <Avatar
                  src={character?.portraitUrl ?? member.avatarUrl}
                  size={64}
                  radius="50%"
                  color="umber"
                  style={{ filter: connected ? "sepia(0.55)" : "grayscale(1) brightness(0.6)" }}
                >
                  {(character?.name ?? member.displayName)[0]}
                </Avatar>
              )}
              <Group gap={4} className={styles.tileIcons}>
                {!seat?.camera && (
                  <Tooltip label={seat ? "Camera off" : "Not in the call"}><IconVideoOff size={14} /></Tooltip>
                )}
                {seat?.micOn ? <IconMicrophone size={14} /> : <IconMicrophoneOff size={14} />}
              </Group>
              {talking && (
                <Badge className={styles.talkBadge} color="blood" size="xs" leftSection={<IconWaveSine size={10} />}>
                  To the Gamemaster
                </Badge>
              )}
            </Box>
            <Box px={8} py={6}>
              <Text className="nx-display" fz="md" lh={1.15} truncate>{character?.name ?? member.displayName}</Text>
              <Text fz={11} c="var(--nx-sepia)" truncate>
                {character ? `${archetypeName(character.archetype)} · ${member.displayName}` : "No character yet"}
                {!connected && " · not here"}
              </Text>
            </Box>
          </UnstyledButton>
        );
      })}

      {Array.from({ length: Math.max(0, SEATS - seated.length) }, (_, i) => (
        <Box key={`empty-${i}`} className={`${styles.tile} ${styles.missing}`}>
          <Box className={styles.video}><Text className="nx-flourish" c="var(--nx-sepia)" fz="sm">An empty chair</Text></Box>
        </Box>
      ))}

      <GamemasterSeat isHost={isHost} />
    </Stack>
  );
}

const STATE_LABEL: Record<string, string> = {
  idle: "At the table",
  listening: "Listening",
  thinking: "Thinking…",
  speaking: "Speaking",
  paused: "Paused by the host",
};

/** The fifth seat: the Gamemaster, audio only. The host brings it in and controls it from here. */
function GamemasterSeat({ isHost }: { isHost: boolean }) {
  const gm = useGamemaster();
  const actions = useGamemasterActions();
  const call = useApi();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();

  const bringIn = async () => {
    setBusy(true);
    const result = await call<boolean>(Http.POST, CallApi.gamemaster);
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
  };

  const present = Boolean(gm.participant);
  return (
    <Box className={`${styles.tile} ${styles.gmTile} ${gm.speaking ? styles.speaking : ""}`}>
      <Box className={styles.video} style={{ height: 64 }}>
        <SightMark size={26} style={gm.speaking ? { filter: "drop-shadow(0 0 8px rgba(184,149,90,0.9))" } : undefined} />
      </Box>
      <Box px={8} py={6}>
        <Group justify="space-between" gap={4} wrap="nowrap">
          <Text className="nx-display" fz="md">The Gamemaster</Text>
          {present && gm.latencyMs !== undefined && (
            <Tooltip label="From releasing the talk key to the first word, last turn">
              <Badge size="xs" variant="outline" color={gm.latencyMs <= 1500 ? "moss.3" : "gold"}>{(gm.latencyMs / 1000).toFixed(1)} s</Badge>
            </Tooltip>
          )}
        </Group>
        <Text fz={11} c="var(--nx-sepia)">
          {present ? STATE_LABEL[gm.state ?? "idle"] ?? gm.state : "Not at the table · audio only"}
        </Text>
        {isHost && !present && (
          <Button mt={6} size="compact-xs" color="blood" fullWidth loading={busy} onClick={bringIn} leftSection={<IconPlayerPlay size={12} />}>
            Bring in the Gamemaster
          </Button>
        )}
        {isHost && present && (
          <Menu position="right-start" withinPortal>
            <Menu.Target>
              <Button mt={6} size="compact-xs" variant="outline" color="gold" fullWidth>Host controls</Button>
            </Menu.Target>
            <Menu.Dropdown>
              {gm.state === "paused" ? (
                <Menu.Item leftSection={<IconPlayerPlay size={14} />} onClick={() => actions.control("resume")}>Resume</Menu.Item>
              ) : (
                <Menu.Item leftSection={<IconPlayerPause size={14} />} onClick={() => actions.control("pause")}>Pause</Menu.Item>
              )}
              <Menu.Item leftSection={<IconRepeat size={14} />} onClick={() => actions.control("repeat")}>Repeat the last reply</Menu.Item>
              <Menu.Item leftSection={<IconScript size={14} />} onClick={() => actions.control("recap")}>Recap the story</Menu.Item>
              <Menu.Item leftSection={<IconArrowForwardUp size={14} />} onClick={() => actions.control("move_on")}>
                Move the story on
              </Menu.Item>
              <Menu.Item leftSection={<IconHourglass size={14} />} onClick={() => actions.control(gm.nudge ? "nudge_off" : "nudge_on")}>
                {gm.nudge ? "Stop nudging after a silence" : "Nudge the table after a silence"}
              </Menu.Item>
              <Menu.Divider />
              <Menu.Item color="blood.4" leftSection={<IconX size={14} />} onClick={() => actions.control("leave")}>Send the Gamemaster away</Menu.Item>
            </Menu.Dropdown>
          </Menu>
        )}
        <ErrorNote message={error} />
      </Box>
    </Box>
  );
}
