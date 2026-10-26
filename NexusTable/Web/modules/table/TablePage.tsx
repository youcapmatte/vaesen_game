"use client";

import { Anchor, Badge, Box, Button, Drawer, Group, Modal, Popover, Select, Stack, Switch, Text, Textarea, Title, Tooltip } from "@mantine/core";
import { IconBook2, IconMail, IconMicrophone, IconNotes, IconPlayerPlay, IconPlayerStop, IconScript, IconSettings } from "@tabler/icons-react";
import Link from "next/link";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { Character } from "@/common/api/character";
import { RollApi, type Roll, type RollOutcome } from "@/common/api/roll";
import type { MysteryAtTable } from "@/common/api/society";
import { noteBody, type PlaySession, SessionApi, type SessionEvent, sessionStamp } from "@/common/api/session";
import type { VaesenRules } from "@/common/api/rules";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { ruleNames, useMe, useRules, useSociety } from "@/common/client/components/hooks";
import { Corners, Divider, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame } from "@/common/client/components/PageFrame";
import { useTableEvent, useTablePresence } from "@/common/client/signalr/useTable";
import { CharacterSheet } from "@/modules/characters/CharacterSheet";
import { CallBar, GmLine, type TranscriptLine, useTableTranscript } from "@/modules/call/CallViews";
import { CallProvider, useGamemaster, useGamemasterActions, useSeatMedia } from "@/modules/call/Call";
import { RollEntry, RollForm, RollOnStage, TestCard } from "@/modules/dice/RollViews";
import { Lobby } from "./Lobby";
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
  const { data: session, mutate: mutateSession } = useApiData<PlaySession | null>(SessionApi.current);
  const { data: sessionLog, mutate: mutateLog } = useApiData<SessionEvent[]>(session ? SessionApi.log(session.id) : null);
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

  // Sessions starting and ending, tests asked for and rolled; and the log as it grows.
  useTableEvent("sessionChanged", () => void mutateSession());
  useTableEvent("sessionEvent", (event) => {
    if (event.sessionId !== session?.id) return;
    void mutateLog((current = []) => [...current.filter((e) => e.id !== event.id), event], { revalidate: false });
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
  // No case on the table: the lobby. Bringing one in turns the same page, on the same call, into the mystery.
  const atTable = view.society.atTable;
  const mine = characters.find((c) => c.id === me.activeCharacterId);
  const rollable = me.role === "Host" ? characters : mine ? [mine] : [];
  const onRolled = (outcome: RollOutcome) => {
    upsertRoll(outcome.roll);
    showOnStage(outcome.roll);
    void mutateSociety();
    void mutateSession();
    // Only the roller tells the Gamemaster, so it hears each roll once.
    void gm.reportRoll(outcome.roll.id);
  };
  // The log, newest first: rolls; the session's stored log (what was said, notes, tests asked for); and the live
  // transcript of lines the log does not have yet.
  const logged = (sessionLog ?? []).filter((e) => LOG_KINDS.includes(e.kind));
  const loggedTexts = new Set(logged.map((e) => e.text.trim()));
  const entries: { at: number; key: string; roll?: Roll; line?: TranscriptLine; event?: SessionEvent }[] = [
    ...(rolls ?? []).map((roll) => ({ at: new Date(roll.createdAt).getTime(), key: roll.id, roll })),
    ...logged.map((event) => ({ at: new Date(event.at).getTime(), key: event.id, event })),
    ...transcript.filter((line) => !loggedTexts.has(line.text.trim())).map((line) => ({ at: line.at, key: line.id, line })),
  ].sort((a, b) => b.at - a.at);
  const tests = session?.pendingTests ?? [];
  const sheet = characters.find((c) => c.id === sheetId);

  return (
    <Box className={styles.layout}>
      <Stack gap="sm" style={{ minHeight: 0 }}>
        <CallBar />
        <Seats members={view.members} presence={presence} media={media} archetypeName={names.archetype} isHost={me.role === "Host"} withGamemaster={Boolean(atTable)} onOpenSheet={setSheetId} />
      </Stack>

      <Box className={styles.stage}>
        <Box className={`${styles.scene} nx-panel`}>
          <Corners />
          {atTable ? (
            <>
              <SessionBar session={session ?? undefined} atTable={atTable} isHost={me.role === "Host"} onChanged={() => void mutateSession()} />
              <SceneCard headquarters={view.society.headquarters} atTable={atTable} isHost={me.role === "Host"} />
              <PushToTalkBar />
            </>
          ) : (
            <Lobby headquarters={view.society.headquarters} isHost={me.role === "Host"} onBrought={() => void mutateSociety()} />
          )}
          {onStage && (
            <Box className={styles.diceOverlay} onClick={() => setOnStage(undefined)}>
              <RollOnStage roll={onStage} rules={rules} />
            </Box>
          )}
        </Box>
        {atTable && <ClueShelf atTable={atTable} />}
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
              entries.map((entry) => entry.event ? (
                <EventLine key={entry.key} event={entry.event} />
              ) : entry.roll ? (
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
          {tests.length > 0 && (
            <Stack gap="xs" mb="sm">
              {tests.map((test) => (
                <TestCard key={test.id} test={test} rules={rules} canRoll={rollable.some((c) => c.id === test.characterId)} onRolled={onRolled} />
              ))}
            </Stack>
          )}
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

/** The session log lines the table shows; rolls show from the rolls themselves, the scribe's lines only to the host. */
const LOG_KINDS: SessionEvent["kind"][] = ["playerSpoke", "gmSaid", "note", "testRequested", "sessionStarted", "sessionEnded"];

/** One stored line of the session: what was said, a note, a test asked for, a session starting or ending. */
function EventLine({ event }: { event: SessionEvent }) {
  const time = new Date(event.at).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  if (event.kind === "sessionStarted" || event.kind === "sessionEnded") {
    return <Text py={6} fz="xs" ta="center" c="var(--nx-sepia)" className="nx-flourish">— {event.text} {time} —</Text>;
  }

  if (event.kind === "note" || event.kind === "testRequested") {
    return (
      <Box py={8} style={{ borderBottom: "1px solid rgba(184,149,90,0.15)" }}>
        <Group justify="space-between" gap={4} wrap="nowrap">
          <Text fz="sm" className="nx-display" c="var(--nx-gold)">
            {event.kind === "note" ? `Note to the Gamemaster · ${event.speaker}` : "The Gamemaster asks for a test"}
          </Text>
          <Text fz={11} c="var(--nx-sepia)" title={`Session clock ${sessionStamp(event.sessionTime)}${event.sentAt ? `; sent at ${new Date(event.sentAt).toLocaleTimeString()}` : ""}`}>{time}</Text>
        </Group>
        <Text fz="sm" c="var(--nx-parchment-2)" style={{ whiteSpace: "pre-wrap" }}>{event.text}</Text>
      </Box>
    );
  }

  const fromGm = event.kind === "gmSaid";
  return <GmLine line={{ id: event.id, at: new Date(event.at).getTime(), fromGm, speaker: fromGm ? "The Gamemaster" : event.speaker, text: event.text }} />;
}

/**
 * The session: which sitting of the mystery this is, and for the host, starting and ending it. Anyone can leave a note
 * for the Gamemaster: what happened, typed or dictated. It is logged with its time and the Gamemaster takes it in.
 */
function SessionBar({ session, atTable, isHost, onChanged }: { session?: PlaySession; atTable?: MysteryAtTable; isHost: boolean; onChanged: () => void }) {
  const call = useApi();
  const gm = useGamemaster();
  const actions = useGamemasterActions();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [noting, setNoting] = useState(false);

  if (!atTable) return null;

  const start = async () => {
    setBusy(true);
    const result = await call<PlaySession>(Http.POST, SessionApi.start);
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    onChanged();
  };

  const end = async () => {
    if (!session) return;
    setBusy(true);
    // The Gamemaster closes the evening and its scribe brings the play state up to date, then the session ends.
    if (gm.participant) await actions.control("end_session");
    const result = await call<PlaySession>(Http.POST, SessionApi.end(session.id));
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    onChanged();
  };

  const started = session ? new Date(session.startedAt).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }) : "";
  return (
    <Box style={{ position: "absolute", top: 12, left: 12, right: 12, zIndex: 2 }}>
      <Group justify="space-between" gap="xs">
        <Badge variant="outline" color={session ? "gold" : "umber.3"} size="lg">
          {session ? `Session ${session.number} · since ${started}` : "No session under way"}
        </Badge>
        <Group gap="xs">
          {session && (
            <Button size="compact-sm" variant="light" color="gold" leftSection={<IconNotes size={14} />} onClick={() => setNoting(true)}>
              Note to the Gamemaster
            </Button>
          )}
          {isHost && !session && (
            <Button size="compact-sm" color="blood" leftSection={<IconPlayerPlay size={14} />} loading={busy} onClick={start}>Start the session</Button>
          )}
          {isHost && session && (
            <Button size="compact-sm" variant="outline" color="blood.4" leftSection={<IconPlayerStop size={14} />} loading={busy} onClick={end}>End the session</Button>
          )}
        </Group>
      </Group>
      <ErrorNote message={error} />
      <NoteModal opened={noting} onClose={() => setNoting(false)} />
    </Box>
  );
}

/** A note to the Gamemaster: typed, or dictated while a button is held. Logged with its time; never answered aloud. */
function NoteModal({ opened, onClose }: { opened: boolean; onClose: () => void }) {
  const call = useApi();
  const gm = useGamemaster();
  const actions = useGamemasterActions();
  const [text, setText] = useState("");
  const [busy, setBusy] = useState(false);
  const [dictating, setDictating] = useState(false);
  const [error, setError] = useState<string>();
  const [sent, setSent] = useState<string>();
  // When the note was begun (opening it, or starting the next one): the moment it is about, on the session clock.
  const begunAt = useRef<number | undefined>(undefined);
  useEffect(() => {
    begunAt.current = opened ? performance.now() : undefined;
  }, [opened]);

  const write = (value: string) => {
    if (begunAt.current === undefined && value.trim()) begunAt.current = performance.now();
    setText(value);
  };

  const save = async () => {
    setBusy(true);
    const result = await call<SessionEvent>(Http.POST, SessionApi.notes, noteBody(text, begunAt.current));
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    if (result.success && result.result) {
      // The Gamemaster reads it back from the log by its id.
      await actions.sendNote(result.result.id);
      begunAt.current = undefined;
      setText("");
      setSent("The note is in the log, and the Gamemaster has it.");
    }
  };

  const startDictating = () => {
    setDictating(true);
    setSent(undefined);
    void actions.startNote();
  };
  const stopDictating = () => {
    if (!dictating) return;
    setDictating(false);
    void actions.endTurn();
    setSent("Dictated: the Gamemaster logs it and takes it in without answering.");
  };

  return (
    <Modal opened={opened} onClose={onClose} title={<Text className="nx-kicker">A note to the Gamemaster</Text>} centered size="lg">
      <Stack gap="sm">
        <Text fz="sm" c="dimmed">
          What happened, a correction, a decision the table made. It goes into the session log at the moment you
          began it, so the Gamemaster knows where in the story it belongs, and takes it as true for the rest of the
          mystery. It doesn&apos;t answer notes aloud.
        </Text>
        <Textarea autosize minRows={3} maxRows={10} maxLength={2000} value={text} onChange={(e) => write(e.currentTarget.value)} placeholder="e.g. Between sessions, Elin wrote to the vicar and he agreed to meet her on Sunday." />
        <Group justify="space-between">
          <Tooltip label={gm.participant ? "Hold while you speak; let go to finish" : "The Gamemaster isn't at the table"}>
            <Button
              variant={dictating ? "filled" : "light"}
              color="blood"
              leftSection={<IconMicrophone size={16} />}
              disabled={!gm.participant}
              onPointerDown={startDictating}
              onPointerUp={stopDictating}
              onPointerLeave={stopDictating}
            >
              {dictating ? "Listening… let go to finish" : "Hold to dictate"}
            </Button>
          </Tooltip>
          <Button color="gold" loading={busy} disabled={!text.trim()} onClick={save}>Write it in the log</Button>
        </Group>
        {sent && <Text fz="sm" c="moss.4">{sent}</Text>}
        <ErrorNote message={error} />
      </Stack>
    </Modal>
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
