"use client";

import { Avatar, Badge, Box, Group, Stack, Text, Tooltip, UnstyledButton } from "@mantine/core";
import { IconMicrophone, IconMicrophoneOff, IconVideoOff, IconWaveSine } from "@tabler/icons-react";
import type { SocietyMember } from "@/common/api/society";
import type { PresenceEntry } from "@/common/client/signalr/useTable";
import { SightMark } from "@/common/client/components/Ornament";
import styles from "./Table.module.css";

const SEATS = 4;

/**
 * The camera tiles down the side: one per member, missing ones shown as empty chairs, then the Gamemaster
 * (audio only). Video comes with LiveKit; until then each tile shows the portrait and who is here.
 */
export function Seats({ members, presence, archetypeName, onOpenSheet }: {
  members: SocietyMember[];
  presence: PresenceEntry[];
  archetypeName: (id?: string) => string;
  onOpenSheet: (characterId: string) => void;
}) {
  const here = new Map(presence.map((p) => [p.accountId, p]));
  const seated = [...members].sort((a, b) => Number(here.has(b.accountId)) - Number(here.has(a.accountId))).slice(0, SEATS);

  return (
    <Stack gap="sm" className={`${styles.seats} nx-scroll`}>
      {seated.map((member) => {
        const presenceEntry = here.get(member.accountId);
        const connected = Boolean(presenceEntry);
        const talking = presenceEntry?.talkingToGm ?? false;
        const character = member.character;
        return (
          <UnstyledButton
            key={member.accountId}
            className={`${styles.tile} ${talking ? styles.talking : ""} ${connected ? "" : styles.missing}`}
            onClick={() => character && onOpenSheet(character.id)}
            disabled={!character}
          >
            <Box className={styles.video}>
              <Avatar
                src={character?.portraitUrl ?? member.avatarUrl}
                size={64}
                radius="50%"
                color="umber"
                style={{ filter: connected ? "sepia(0.55)" : "grayscale(1) brightness(0.6)" }}
              >
                {(character?.name ?? member.displayName)[0]}
              </Avatar>
              <Group gap={4} className={styles.tileIcons}>
                <Tooltip label="Camera comes with the call (LiveKit)"><IconVideoOff size={14} /></Tooltip>
                {connected ? <IconMicrophone size={14} /> : <IconMicrophoneOff size={14} />}
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

      <Box className={`${styles.tile} ${styles.gmTile}`}>
        <Box className={styles.video} style={{ height: 64 }}>
          <SightMark size={26} />
        </Box>
        <Box px={8} py={6}>
          <Text className="nx-display" fz="md">The Gamemaster</Text>
          <Text fz={11} c="var(--nx-sepia)">Audio only · joins when the host starts a mystery</Text>
        </Box>
      </Box>
    </Stack>
  );
}
