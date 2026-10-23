"use client";

import { Badge, Box, Button, Container, Group, NumberInput, Stack, Text } from "@mantine/core";
import { IconArrowBackUp, IconDice5 } from "@tabler/icons-react";
import { useEffect, useRef, useState } from "react";
import { Corners } from "@/common/client/components/Ornament";
import { PageFrame, PageTitle } from "@/common/client/components/PageFrame";
import { Die, DieFace } from "./Die";

const MAX_DICE = 15;
const TUMBLE_MS = 1100;
const STAGGER_MS = 70;

/** A fair d6 from the browser's crypto source: this is a demo, the table's rolls come from the server. */
function d6() {
  const buffer = new Uint32Array(1);
  // Reject the top of the range so every face is equally likely.
  do {
    crypto.getRandomValues(buffer);
  } while (buffer[0] >= 4294967292);
  return (buffer[0] % 6) + 1;
}

interface Throw {
  id: number;
  dice: number[];
  /** Which dice were thrown in the latest throw or push: only those tumble. */
  thrownAt: number[];
  pushed: boolean;
  required: number;
}

const sixes = (dice: number[]) => dice.filter((d) => d === 6).length;

/** Try the dice: choose the pool and the sixes needed, throw, and push a failed roll as the rules allow. */
export function DicePage() {
  return (
    <PageFrame>
      <DiceTray />
    </PageFrame>
  );
}

function DiceTray() {
  const [count, setCount] = useState(4);
  const [required, setRequired] = useState(1);
  const [current, setCurrent] = useState<Throw>();
  const [settled, setSettled] = useState(true);
  const [log, setLog] = useState<Throw[]>([]);
  const nextId = useRef(1);

  // The outcome shows once the last die has landed.
  useEffect(() => {
    if (!current) return;
    setSettled(false);
    const id = window.setTimeout(() => setSettled(true), TUMBLE_MS + STAGGER_MS * current.dice.length);
    return () => window.clearTimeout(id);
  }, [current]);

  const record = (t: Throw) => setLog((previous) => [t, ...previous.filter((p) => p.id !== t.id)].slice(0, 8));

  const roll = () => {
    const id = nextId.current++;
    const t: Throw = { id, dice: Array.from({ length: count }, d6), thrownAt: Array.from({ length: count }, () => id), pushed: false, required };
    setCurrent(t);
    record(t);
  };

  // Pushing rerolls every die that is not a six; the sixes stay where they lie.
  const push = () => {
    if (!current) return;
    const stamp = nextId.current++;
    const t: Throw = {
      ...current,
      dice: current.dice.map((d) => (d === 6 ? d : d6())),
      thrownAt: current.dice.map((d, i) => (d === 6 ? current.thrownAt[i] : stamp)),
      pushed: true,
    };
    setCurrent(t);
    record(t);
  };

  const found = current ? sixes(current.dice) : 0;
  const success = current ? found >= current.required : false;
  const size = current && current.dice.length > 10 ? 46 : 58;

  return (
    <Container size="md">
      <PageTitle kicker="Try the dice" title="The Dice">
        Choose how many dice you hold and how many sixes the deed needs, then throw. A failed roll may be pushed once:
        every die that is not a six is thrown again. At the table, a push costs a Condition.
      </PageTitle>

      <Box className="nx-panel" p="lg" mb="lg">
        <Corners />
        <Group justify="center" align="flex-end" gap="lg" wrap="wrap">
          <NumberInput label="Dice in the pool" value={count} onChange={(v) => setCount(Math.min(MAX_DICE, Math.max(1, Number(v) || 1)))} min={1} max={MAX_DICE} w={150} />
          <NumberInput label="Sixes needed" value={required} onChange={(v) => setRequired(Math.min(5, Math.max(1, Number(v) || 1)))} min={1} max={5} w={150} />
          <Button size="md" color="blood" leftSection={<IconDice5 size={18} />} onClick={roll} disabled={!settled}>
            Throw the dice
          </Button>
        </Group>
      </Box>

      {/* Clipped, so the dice drop in from the tray's top edge instead of falling across the page. */}
      <Box className="nx-panel" p="xl" mih={260} style={{ display: "flex", flexDirection: "column", justifyContent: "center", overflow: "hidden" }}>
        <Corners />
        {current ? (
          <Stack align="center" gap="lg">
            <Group justify="center" gap="md" style={{ maxWidth: 640 }}>
              {current.dice.map((value, i) => (
                <Die
                  key={i}
                  value={value}
                  size={size}
                  throwKey={`${current.thrownAt[i]}-${i}`}
                  delay={current.thrownAt[i] === current.thrownAt.reduce((a, b) => Math.max(a, b), 0) ? i * STAGGER_MS : 0}
                />
              ))}
            </Group>
            <Box mih={70} ta="center">
              {settled && (
                <Stack align="center" gap={6}>
                  <Group gap="xs" justify="center">
                    <Badge size="lg" color={success ? "moss.5" : "blood"} variant={success ? "filled" : "outline"}>
                      {success ? "Success" : "Failure"}
                    </Badge>
                    <Text fz="sm" c="var(--nx-parchment-2)">
                      {found} {found === 1 ? "six" : "sixes"} of {current.required} needed{current.pushed ? ", pushed" : ""}
                      {success && found > current.required ? ` · ${found - current.required} extra to spend` : ""}
                    </Text>
                  </Group>
                  {!success && !current.pushed && (
                    <Button size="xs" variant="light" color="blood" leftSection={<IconArrowBackUp size={14} />} onClick={push}>
                      Push the roll
                    </Button>
                  )}
                </Stack>
              )}
            </Box>
          </Stack>
        ) : (
          <Text ta="center" className="nx-flourish" c="var(--nx-sepia)">The dice wait in the cup.</Text>
        )}
      </Box>

      {log.length > 0 && (
        <Stack gap={6} mt="lg">
          <Text className="nx-kicker">Recent throws</Text>
          {log.map((t) => {
            const n = sixes(t.dice);
            return (
              <Group key={t.id} gap="sm" wrap="nowrap">
                <Group gap={4} wrap="wrap">
                  {t.dice.map((d, i) => <DieFace key={i} value={d} size={18} faded={d !== 6} />)}
                </Group>
                <Text fz="xs" c="var(--nx-ink-soft)" style={{ whiteSpace: "nowrap" }}>
                  {n}/{t.required} {n >= t.required ? "success" : "failure"}{t.pushed ? " (pushed)" : ""}
                </Text>
              </Group>
            );
          })}
        </Stack>
      )}
    </Container>
  );
}
