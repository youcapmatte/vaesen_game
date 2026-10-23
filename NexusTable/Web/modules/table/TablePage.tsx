"use client";

import { Anchor, Badge, Box, Button, Drawer, Group, Modal, Popover, Select, Stack, Switch, Text, Title, Tooltip } from "@mantine/core";
import { IconBook2, IconMail, IconMicrophone, IconScript, IconSettings } from "@tabler/icons-react";
import Link from "next/link";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { Character } from "@/common/api/character";
import { RollApi, type Roll, type RollOutcome } from "@/common/api/roll";
import type { MysteryAtTable } from "@/common/api/society";
import type { VaesenRules } from "@/common/api/rules";
import { useApiData } from "@/common/client/core/Api";
import { ruleNames, useMe, useRules, useSociety } from "@/common/client/components/hooks";
import { Corners, Divider, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame } from "@/common/client/components/PageFrame";
import { useTableEvent, useTablePresence } from "@/common/client/signalr/useTable";
import { CharacterSheet } from "@/modules/characters/CharacterSheet";
import { CallBar, GmLine, type TranscriptLine, useTableTranscript } from "@/modules/call/CallViews";
import { CallProvider, useGamemaster, useGamemasterActions, useSeatMedia } from "@/modules/call/Call";
import { RollEntry, RollForm, RollOnStage } from "@/modules/dice/RollViews";
import { Seats } from "./Seats";
import styles from "./Table.module.css";
import { keyLabel, usePushToTalk } from "./usePushToTalk";

const STAGE_SECONDS = 7;

export function TablePage() {
  return (
    <PageFrame fullBleed>
      <CallProvider>
        <TableContent />
      </CallProvider>
    </PageFrame>
  );
}

function TableContent() {
  const { data: rules } = useRules();
  const { data: me } = useMe();
  const { data: view, error, mutate: mutateSociety } = useSociety();
  const { data: rolls, mutate: mutateRolls } = useApiData<Roll[]>(RollApi.rolls);
  const { presence } = useTablePresence();
  const media = useSeatMedia();
  const gm = useGamemasterActions();
  const transcript = useTableTranscript(view?.members ?? []);
  const [onStage, setOnStage] = useState<Roll>();
  const [sheetId, setSheetId] = useState<string>();
  const stageTimer = useRef<number | undefined>(undefined);

  const showOnStage = useCallback((roll: Roll) => {
    setOnStage(roll);
    window.clearTimeout(stageTimer.current);
    stageTimer.current = window.setTimeout(() => setOnStage(undefined), STAGE_SECONDS * 1000);
  }, []);

  const upsertRoll = useCallback((roll: Roll) => {
    void mutateRolls((current = []) => [roll, ...current.filter((r) => r.id !== roll.id)], { revalidate: false });
  }, [mutateRolls]);

  // Everyone's rolls, including our own (which also come back from the API call).
  useTableEvent("rollMade", (roll) => {
    upsertRoll(roll);
    showOnStage(roll);
  });

  useEffect(() => () => window.clearTimeout(stageTimer.current), []);

  const characters = useMemo(() => view?.members.flatMap((m) => (m.character ? [m.character] : [])) ?? [], [view]);

  if (error) {
    return <Box p="xl"><ErrorNote message={error.message} /></Box>;
  }
  if (!rules || !me || !view) {
    return <FullPageLoader label="Pulling up the chairs…" />;
  }

  const names = ruleNames(rules);
  const mine = characters.find((c) => c.id === me.activeCharacterId);
  const rollable = me.role === "Host" ? characters : mine ? [mine] : [];
  const onRolled = (outcome: RollOutcome) => {
    upsertRoll(outcome.roll);
    showOnStage(outcome.roll);
    void mutateSociety();
    // Only the roller tells the Gamemaster, so it hears each roll once.
    void gm.reportRoll(outcome.roll.id);
  };
  // The log: rolls and the Gamemaster's conversation, newest first.
  const entries: { at: number; key: string; roll?: Roll; line?: TranscriptLine }[] = [
    ...(rolls ?? []).map((roll) => ({ at: new Date(roll.createdAt).getTime(), key: roll.id, roll })),
    ...transcript.map((line) => ({ at: line.at, key: line.id, line })),
  ].sort((a, b) => b.at - a.at);
  const sheet = characters.find((c) => c.id === sheetId);

  return (
    <Box className={styles.layout}>
      <Stack gap="sm" style={{ minHeight: 0 }}>
        <CallBar />
        <Seats members={view.members} presence={presence} media={media} archetypeName={names.archetype} isHost={me.role === "Host"} onOpenSheet={setSheetId} />
      </Stack>

      <Box className={styles.stage}>
        <Box className={`${styles.scene} nx-panel`}>
          <Corners />
          <SceneCard headquarters={view.society.headquarters} atTable={view.society.atTable} isHost={me.role === "Host"} />
          <PushToTalkBar />
          {onStage && (
            <Box className={styles.diceOverlay} onClick={() => setOnStage(undefined)}>
              <RollOnStage roll={onStage} rules={rules} />
            </Box>
          )}
        </Box>
        <ClueShelf atTable={view.society.atTable} />
      </Box>

      <Box className={styles.side}>
        <Box className="nx-panel" p="md" style={{ display: "flex", flexDirection: "column", minHeight: 0, flex: 1 }}>
          <Group justify="space-between" mb={4}>
            <Text className="nx-kicker">The log</Text>
            {mine && (
              <Button size="compact-sm" variant="subtle" color="gold" leftSection={<IconBook2 size={14} />} onClick={() => setSheetId(mine.id)}>
                My sheet
              </Button>
            )}
          </Group>
          <Box className={`${styles.log} nx-scroll`}>
            {entries.length ? (
              entries.map((entry) => entry.roll ? (
                <RollEntry
                  key={entry.key}
                  roll={entry.roll}
                  rules={rules}
                  me={me}
                  character={characters.find((c) => c.id === entry.roll!.characterId)}
                  onPushed={onRolled}
                />
              ) : (
                <GmLine key={entry.key} line={entry.line!} />
              ))
            ) : (
              <Text c="var(--nx-sepia)" className="nx-flourish" fz="sm" mt="md" ta="center">
                Every word of the Gamemaster, every roll and every clue will be written here.
              </Text>
            )}
          </Box>
        </Box>

        <Box className="nx-panel" p="md">
          <Text className="nx-kicker" mb="xs">Roll the dice</Text>
          <RollForm rules={rules} characters={rollable} defaultCharacterId={mine?.id} onRolled={onRolled} />
        </Box>
      </Box>

      <Drawer
        opened={Boolean(sheet)}
        onClose={() => setSheetId(undefined)}
        position="right"
        size="xl"
        title={<Text className="nx-kicker">Character sheet</Text>}
        styles={{ content: { background: "var(--nx-night)" }, header: { background: "var(--nx-night)" } }}
      >
        {sheet && (
          <SheetInDrawer character={sheet} rules={rules} canEdit={sheet.accountId === me.id || me.role === "Host"} onChange={() => void mutateSociety()} />
        )}
      </Drawer>
    </Box>
  );
}

function SheetInDrawer({ character, rules, canEdit, onChange }: { character: Character; rules: VaesenRules; canEdit: boolean; onChange: () => void }) {
  // The Society view carries no dark secret for others; the owner's sheet page has the full one.
  return <CharacterSheet character={character} rules={rules} canEdit={canEdit} canSeeSecret={canEdit && Boolean(character.darkSecret)} onChange={onChange} compact />;
}

/**
 * The centre of the table: the current scene. With a case on the table, its title and how it began; before that, the
 * Society's own rooms. The host also gets the way to the case files.
 */
function SceneCard({ headquarters, atTable, isHost }: { headquarters: string; atTable?: MysteryAtTable; isHost: boolean }) {
  const gm = useGamemaster();
  const text = atTable
    ? gm.participant
      ? "The Gamemaster has the case. Hold the talk key and speak to them; let go and they answer. Everything else you say stays among the Society."
      : "A letter has come to the Society; it lies with the clues below. Join the call, and when the host brings in the Gamemaster, the mystery begins."
    : gm.participant
      ? "Hold the talk key and speak to them; let go and they answer. Everything else you say stays among the Society."
      : "The fire is low and the letters on the table are unopened. Join the call, and when the host brings in the Gamemaster, it takes the fifth seat and paints the first scene: the place, the people, the wrongness that brought you.";

  return (
    <Stack align="center" gap="sm" maw={560} ta="center" p="xl">
      <Text className="nx-kicker">{atTable ? "The mystery at the table" : <>The Society&apos;s rooms · {headquarters}</>}</Text>
      <Title order={2}>{atTable ? atTable.title : gm.participant ? "The Gamemaster is at the table" : "No mystery has begun"}</Title>
      <Divider width={200} />
      <Text className="nx-flourish nx-dropcap" ta="left" c="var(--nx-parchment-2)">{text}</Text>
      {isHost && (
        <Stack gap={2} align="center">
          <Anchor component={Link} href="/mysteries" fz="sm" c="gold.4">
            <Group gap={4} component="span"><IconScript size={14} />{atTable ? "Open the case files" : "Choose a case for the table"}</Group>
          </Anchor>
          {atTable && gm.participant && (
            <Text fz="xs" c="var(--nx-sepia)">The Gamemaster reads the case when it joins: to change cases, send it away and bring it in again.</Text>
          )}
        </Stack>
      )}
    </Stack>
  );
}

/** Found clues and handouts, laid out like papers on the table. The hook that brought the Society in comes first. */
function ClueShelf({ atTable }: { atTable?: MysteryAtTable }) {
  const [reading, setReading] = useState(false);
  return (
    <Box className={`${styles.shelf} nx-panel`} p="md">
      <Group justify="space-between" mb="xs">
        <Text className="nx-kicker">Clues &amp; handouts</Text>
        <Badge variant="outline" color="umber.3">{atTable ? "1" : "0"} found</Badge>
      </Group>
      <Group gap="md">
        {atTable && (
          <Tooltip label="Read the letter" withArrow>
            <Box
              component="button"
              type="button"
              className="nx-parchment"
              w={120}
              h={72}
              p={8}
              onClick={() => setReading(true)}
              style={{ transform: "rotate(-2deg)", display: "grid", placeItems: "center", cursor: "pointer", border: 0 }}
            >
              <Stack gap={2} align="center">
                <IconMail size={18} color="var(--nx-blood)" />
                <Text fz={11} c="var(--nx-ink)" lh={1.1} lineClamp={2}>{atTable.title}</Text>
              </Stack>
            </Box>
          </Tooltip>
        )}
        {(atTable ? ["A map", "A testimony"] : ["A letter", "A map", "A testimony"]).map((label, i) => (
          <Box
            key={label}
            className="nx-parchment"
            w={120}
            h={72}
            p={8}
            style={{ opacity: 0.22, transform: `rotate(${[-2, 1.5, -1][i]}deg)`, display: "grid", placeItems: "center" }}
          >
            <SightMark size={16} style={{ color: "var(--nx-ink)" }} />
          </Box>
        ))}
        <Text className="nx-flourish" c="var(--nx-sepia)" fz="sm" maw={260}>What you uncover will be pinned here for everyone to read.</Text>
      </Group>
      {atTable && (
        <Modal opened={reading} onClose={() => setReading(false)} title={<Text className="nx-kicker">How it began</Text>} centered size="lg">
          <Box className="nx-parchment" p="xl">
            <Corners color="var(--nx-blood)" />
            <Title order={3} mb="sm">{atTable.title}</Title>
            <Text className="nx-flourish" c="var(--nx-ink)" style={{ whiteSpace: "pre-wrap" }}>{atTable.hookText}</Text>
          </Box>
        </Modal>
      )}
    </Box>
  );
}

const KEY_CHOICES = ["Backquote", "KeyT", "KeyV", "CapsLock", "Mouse3", "Mouse4"];

/** Shows push-to-talk: the key, whether you're talking to the Gamemaster, and its settings. */
function PushToTalkBar() {
  const gm = useGamemasterActions();
  const { talking, settings, save } = usePushToTalk({ onStart: () => void gm.startTurn(), onEnd: () => void gm.endTurn() });
  return (
    <Box className={styles.ptt}>
      <Group gap={6} wrap="nowrap" className="nx-panel" px="md" py={6} style={talking ? { borderColor: "var(--nx-blood-bright)", boxShadow: "0 0 24px rgba(177,54,40,0.6)" } : undefined}>
        <IconMicrophone size={16} color={talking ? "var(--nx-blood-bright)" : "var(--nx-sepia)"} />
        <Text component="div" fz="sm" c={talking ? "var(--nx-parchment)" : "var(--nx-parchment-2)"}>
          {talking ? "Speaking to the Gamemaster…" : (
            <>
              {settings.toggle ? "Press" : "Hold"} <Badge size="sm" variant="outline" color="gold">{keyLabel(settings.key)}</Badge> to speak to the Gamemaster
              {!gm.present && <Text span c="var(--nx-sepia)" fz="xs"> (not at the table yet)</Text>}
            </>
          )}
        </Text>
        <Popover position="top" withArrow>
          <Popover.Target>
            <Button size="compact-xs" variant="subtle" color="umber.2" aria-label="Push-to-talk settings"><IconSettings size={14} /></Button>
          </Popover.Target>
          <Popover.Dropdown>
            <Stack gap="xs" w={220}>
              <Select
                size="xs"
                label="Talk key"
                value={settings.key}
                data={KEY_CHOICES.map((k) => ({ value: k, label: keyLabel(k) }))}
                onChange={(v) => v && save({ ...settings, key: v })}
              />
              <Switch size="xs" label="Toggle instead of hold" checked={settings.toggle} onChange={(e) => save({ ...settings, toggle: e.currentTarget.checked })} />
              <Text fz={11} c="dimmed">Works while this tab has focus. Table talk is never sent to the Gamemaster.</Text>
            </Stack>
          </Popover.Dropdown>
        </Popover>
      </Group>
    </Box>
  );
}
