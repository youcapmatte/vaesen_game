"use client";

import { Badge, Box, Button, Group, Menu, NumberInput, SegmentedControl, Select, Stack, Switch, Text, TextInput, Tooltip } from "@mantine/core";
import { IconArrowBackUp, IconDice5 } from "@tabler/icons-react";
import { useMemo, useState } from "react";
import type { Account } from "@/common/api/account";
import type { Character } from "@/common/api/character";
import { RollApi, type Roll, type RollOutcome, sixesNeeded, succeeded } from "@/common/api/roll";
import type { TestRequest } from "@/common/api/session";
import type { VaesenRules } from "@/common/api/rules";
import { Http, useApi } from "@/common/client/core/Api";
import { ruleNames } from "@/common/client/components/hooks";
import { ErrorNote } from "@/common/client/components/PageFrame";
import { Die, DieFace, TUMBLE_MS } from "./Die";
import { canPush, finalDice, MAX_MODIFIER, previewPool, pushConditions } from "./pool";

function rollTitle(rules: VaesenRules, roll: Roll) {
  const names = ruleNames(rules);
  return roll.skill ? names.skill(roll.skill) : `${names.attribute(roll.attribute)} test`;
}

/** Success or failure against the sixes the roll needed; with more than one needed, how many of them came. */
function Outcome({ roll }: { roll: Roll }) {
  const needed = sixesNeeded(roll);
  const count = needed > 1 ? `${roll.successes} of ${needed} sixes` : `${roll.successes} six${roll.successes === 1 ? "" : "es"}`;
  return succeeded(roll) ? (
    <Badge color="moss.5" variant="filled">Success · {count}</Badge>
  ) : (
    <Badge color="blood" variant="outline">Failure{needed > 1 || roll.successes > 0 ? ` · ${count}` : ""}</Badge>
  );
}

/** Push a failed roll: choose the Condition it costs (or break, when none is left). */
export function PushButton({ roll, rules, character, onPushed, size = "xs" }: {
  roll: Roll;
  rules: VaesenRules;
  character: Character;
  onPushed?: (outcome: RollOutcome) => void;
  size?: "xs" | "sm";
}) {
  const call = useApi();
  const [error, setError] = useState<string>();
  const [busy, setBusy] = useState(false);
  const options = pushConditions(rules, character, roll);

  const push = async (condition: string) => {
    setBusy(true);
    const result = await call<RollOutcome>(Http.POST, RollApi.push(roll.id), { condition });
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    if (result.success && result.result) onPushed?.(result.result);
  };

  return (
    <Stack gap={2}>
      <Menu position="bottom-start" withinPortal>
        <Menu.Target>
          <Button size={size} color="blood" variant="light" loading={busy} leftSection={<IconArrowBackUp size={14} />}>
            Push the roll
          </Button>
        </Menu.Target>
        <Menu.Dropdown>
          <Menu.Label>Reroll every die that isn&apos;t a six, and suffer…</Menu.Label>
          {options.length > 0 ? (
            options.map((c) => <Menu.Item key={c.id} onClick={() => push(c.id)}>{c.name}</Menu.Item>)
          ) : (
            <Menu.Item color="blood.4" onClick={() => push("")}>…being Broken</Menu.Item>
          )}
        </Menu.Dropdown>
      </Menu>
      <ErrorNote message={error} />
    </Stack>
  );
}

/** One roll in the log: who, what, how the pool was built, the faces, the outcome. */
export function RollEntry({ roll, rules, me, character, onPushed }: {
  roll: Roll;
  rules: VaesenRules;
  me?: Account;
  character?: Character;
  onPushed?: (outcome: RollOutcome) => void;
}) {
  const mine = me && (roll.accountId === me.id || me.role === "Host");
  const time = new Date(roll.createdAt).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  return (
    <Box py={8} style={{ borderBottom: "1px solid rgba(184,149,90,0.15)" }}>
      <Group justify="space-between" gap={4} wrap="nowrap">
        <Text fz="sm" lh={1.2}>
          <Text span className="nx-display">{roll.characterName}</Text>
          <Text span c="var(--nx-sepia)"> rolls </Text>
          <Text span className="nx-display" c="var(--nx-gold)">{rollTitle(rules, roll)}</Text>
        </Text>
        <Text fz={11} c="var(--nx-sepia)">{time}</Text>
      </Group>
      {roll.reason && <Text fz="xs" c="var(--nx-parchment-2)" className="nx-flourish">{roll.reason}</Text>}
      <Tooltip label={roll.pool.map((p) => `${p.label} ${p.dice > 0 ? "+" : ""}${p.dice}`).join(" · ")}>
        <Group gap={3} mt={6} wrap="wrap">
          {roll.dice.map((d, i) => <DieFace key={i} value={d} faded={roll.pushed && d !== 6} />)}
        </Group>
      </Tooltip>
      {roll.pushedDice && (
        <Group gap={3} mt={4} wrap="wrap">
          <Text fz={11} c="var(--nx-sepia)" mr={4}>pushed</Text>
          {roll.pushedDice.map((d, i) => <DieFace key={i} value={d} />)}
        </Group>
      )}
      <Group gap={6} mt={6}>
        <Outcome roll={roll} />
        {roll.pushCondition && <Badge color="umber.4" variant="outline" size="sm">{ruleNames(rules).condition(roll.pushCondition)}</Badge>}
        {mine && character && canPush(roll) && <PushButton roll={roll} rules={rules} character={character} onPushed={onPushed} />}
      </Group>
    </Box>
  );
}

/** The dice landing on the stage for everyone, then the outcome. */
export function RollOnStage({ roll, rules }: { roll: Roll; rules: VaesenRules }) {
  const dice = finalDice(roll);
  const throwKey = `${roll.id}-${roll.pushed ? "pushed" : "first"}`;
  const size = dice.length > 10 ? 38 : 52;
  return (
    <Stack align="center" gap="md" maw={620} px="md">
      <Text className="nx-kicker">
        {roll.characterName} · {rollTitle(rules, roll)} · {dice.length} dice{sixesNeeded(roll) > 1 ? ` · ${sixesNeeded(roll)} sixes needed` : ""}{roll.pushed ? " · pushed" : ""}
      </Text>
      <Group gap={14} justify="center" wrap="wrap">
        {dice.map((d, i) => (
          <Die
            key={`${throwKey}-${i}`}
            value={d}
            size={size}
            throwKey={throwKey}
            // On a push the sixes stay where they lay.
            delay={roll.pushed && roll.dice[i] === 6 ? 0 : i * 70}
            dim={false}
          />
        ))}
      </Group>
      <Group gap="xs" style={{ animation: `nx-fade-in 400ms ${TUMBLE_MS + 70 * dice.length}ms both` }}>
        <Outcome roll={roll} />
      </Group>
    </Stack>
  );
}

const testTitle = (rules: VaesenRules, test: TestRequest) => {
  const names = ruleNames(rules);
  return test.skill ? names.skill(test.skill) : `${names.attribute(test.attribute)} test`;
};

/**
 * A test the Gamemaster asked for: what to roll, how hard, the sixes it needs and the dice the character has. The
 * roller (or the host) throws it with one click; the roll takes its skill, difficulty and sixes from the test.
 */
export function TestCard({ test, rules, canRoll, onRolled }: {
  test: TestRequest;
  rules: VaesenRules;
  canRoll: boolean;
  onRolled: (outcome: RollOutcome) => void;
}) {
  const call = useApi();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const roll = async () => {
    setBusy(true);
    const result = await call<RollOutcome>(Http.POST, RollApi.rolls, { characterId: test.characterId, testId: test.id });
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    if (result.success && result.result) onRolled(result.result);
  };
  return (
    <Box className="nx-parchment" p="sm" style={{ borderLeft: "3px solid var(--nx-blood)" }}>
      <Text fz={11} className="nx-kicker" c="var(--nx-blood)">The Gamemaster asks for a test</Text>
      <Text fz="sm" c="var(--nx-ink)" lh={1.25}>
        <Text span className="nx-display">{test.characterName}</Text>: {testTitle(rules, test)}
        {test.difficulty !== 0 && <> {test.difficulty > 0 ? "+" : ""}{test.difficulty}</>}
      </Text>
      {test.reason && <Text fz="xs" c="var(--nx-ink-soft)" className="nx-flourish">{test.reason}</Text>}
      <Group justify="space-between" mt={6} wrap="nowrap" align="center">
        <Group gap={6} wrap="nowrap">
          <Tooltip label={test.pool.map((p) => `${p.label} ${p.dice > 0 ? "+" : ""}${p.dice}`).join(" · ")}>
            <Badge color="umber.6" variant="filled">{test.poolSize} dice</Badge>
          </Tooltip>
          <Badge color={test.successesNeeded > 1 ? "blood" : "umber.4"} variant="outline">
            {test.successesNeeded} six{test.successesNeeded === 1 ? "" : "es"} needed
          </Badge>
        </Group>
        {canRoll && (
          <Button size="xs" color="blood" leftSection={<IconDice5 size={14} />} loading={busy} onClick={roll}>Roll it</Button>
        )}
      </Group>
      <ErrorNote message={error} />
    </Box>
  );
}

/** Building and throwing a roll for one's own character (the host can roll for anyone). */
export function RollForm({ rules, characters, defaultCharacterId, onRolled }: {
  rules: VaesenRules;
  characters: Character[];
  defaultCharacterId?: string;
  onRolled: (outcome: RollOutcome) => void;
}) {
  const call = useApi();
  const [characterId, setCharacterId] = useState<string | undefined>(defaultCharacterId ?? characters[0]?.id);
  const [test, setTest] = useState<string | null>(null);
  const [difficulty, setDifficulty] = useState("0");
  const [bonus, setBonus] = useState(0);
  const [useAdvantage, setUseAdvantage] = useState(false);
  const [needed, setNeeded] = useState(1);
  const [reason, setReason] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();

  const character = characters.find((c) => c.id === (characterId ?? defaultCharacterId)) ?? characters[0];
  const [kind, id] = (test ?? "").split(":");
  const choice = {
    skill: kind === "skill" ? id : undefined,
    attribute: kind === "attribute" ? id : undefined,
    difficulty: Number(difficulty),
    bonus,
    useAdvantage,
  };
  const pool = character && test ? previewPool(rules, character, choice) : undefined;

  const testOptions = useMemo(() => [
    ...rules.attributes.map((a) => ({
      group: a.name,
      items: rules.skills.filter((s) => s.attribute === a.id).map((s) => ({
        value: `skill:${s.id}`,
        label: `${s.name} (${(character?.attributes[a.id] ?? 0) + (character?.skills[s.id] ?? 0)})`,
      })),
    })),
    {
      group: "Attribute tests (Fear and the like)",
      items: rules.attributes.map((a) => ({ value: `attribute:${a.id}`, label: `${a.name} (${character?.attributes[a.id] ?? 0})` })),
    },
  ], [rules, character]);

  if (!character) {
    return <Text c="var(--nx-sepia)" className="nx-flourish" fz="sm">Create a character to roll dice.</Text>;
  }

  const roll = async () => {
    setBusy(true);
    const result = await call<RollOutcome>(Http.POST, RollApi.rolls, { characterId: character.id, ...choice, successesNeeded: needed, reason: reason || undefined });
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    if (result.success && result.result) {
      onRolled(result.result);
      setUseAdvantage(false);
      setNeeded(1);
      setReason("");
    }
  };

  return (
    <Stack gap="xs">
      {(characters.length > 1 || !defaultCharacterId) && (
        <Select
          size="xs"
          label="Rolling for"
          value={character.id}
          data={characters.map((c) => ({ value: c.id, label: c.name }))}
          onChange={(v) => v && setCharacterId(v)}
        />
      )}
      <Select size="sm" placeholder="What are you rolling?" value={test} data={testOptions} onChange={setTest} searchable maxDropdownHeight={320} />
      <Group gap="xs" grow align="flex-end">
        <Box>
          <Text fz={11} c="var(--nx-sepia)" mb={2}>Difficulty from the Gamemaster</Text>
          <SegmentedControl
            size="xs"
            fullWidth
            value={difficulty}
            onChange={setDifficulty}
            data={[-3, -2, -1, 0, 1, 2, 3].map((d) => ({ value: String(d), label: d > 0 ? `+${d}` : String(d) }))}
          />
        </Box>
      </Group>
      <Group gap="xs" align="flex-end" wrap="nowrap">
        <NumberInput size="xs" label="Gear & help" value={bonus} min={-MAX_MODIFIER} max={MAX_MODIFIER} onChange={(v) => setBonus(Number(v) || 0)} w={100} />
        <NumberInput size="xs" label="Sixes needed" value={needed} min={1} max={5} onChange={(v) => setNeeded(Math.min(5, Math.max(1, Number(v) || 1)))} w={100} />
        <Tooltip label={character.advantage ?? "Write an Advantage on your sheet first"} disabled={false}>
          <Switch
            size="sm"
            color="gold"
            label="Advantage +2"
            checked={useAdvantage}
            disabled={!character.advantage}
            onChange={(e) => setUseAdvantage(e.currentTarget.checked)}
          />
        </Tooltip>
      </Group>
      <TextInput size="xs" placeholder="Why? e.g. searching the vicarage library" value={reason} onChange={(e) => setReason(e.currentTarget.value)} />
      <Group justify="space-between" wrap="nowrap" align="flex-end">
        <Text fz="xs" c="var(--nx-parchment-2)" style={{ flex: 1 }}>
          {pool ? <>{pool.parts.map((p) => `${p.label} ${p.dice > 0 ? "+" : ""}${p.dice}`).join(" · ")} = <b>{pool.size} dice</b>{needed > 1 ? `, ${needed} sixes needed` : ""}</> : "Choose a skill."}
        </Text>
        <Button color="blood" leftSection={<IconDice5 size={18} />} disabled={!pool} loading={busy} onClick={roll} style={{ flex: "none" }}>
          Roll
        </Button>
      </Group>
      <ErrorNote message={error} />
    </Stack>
  );
}
