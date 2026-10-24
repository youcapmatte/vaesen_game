"use client";

import { Anchor, Badge, Box, Button, Container, Group, Loader, SimpleGrid, Stack, Text, ThemeIcon, Title } from "@mantine/core";
import { IconAlertTriangle, IconCandle, IconCheck, IconFeather, IconFlask, IconLock, IconScript } from "@tabler/icons-react";
import Link from "next/link";
import { useEffect, useRef, useState } from "react";
import { GenerationSteps, InWorldDate, levelName, MysteryApi, type CaseStatus, type CaseSummary, type MysteryJob } from "@/common/api/mystery";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { useMe, useSociety } from "@/common/client/components/hooks";
import { Corners, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame, PageTitle } from "@/common/client/components/PageFrame";
import { TableButton } from "./TableButton";

/** The host's case files: write a new mystery with the engine, and the cases written so far. Host only. */
export function CaseFilesPage() {
  return (
    <PageFrame>
      <CaseFilesContent />
    </PageFrame>
  );
}

function CaseFilesContent() {
  const { data: me } = useMe();
  if (!me) {
    return <FullPageLoader label="Finding the key to the archive…" />;
  }
  if (me.role !== "Host") {
    return <HostOnly />;
  }
  return <HostCaseFiles />;
}

export function HostOnly() {
  return (
    <Container size="sm">
      <Box className="nx-panel" p="xl" ta="center">
        <Corners />
        <Stack align="center" gap="xs">
          <IconLock size={28} color="var(--nx-gold)" />
          <Title order={3}>The case files are locked</Title>
          <Text className="nx-flourish" c="var(--nx-parchment-2)">Only the host keeps the key. What is written there, the players must find for themselves.</Text>
          <Button component={Link} href="/society" variant="subtle" color="gold" mt="sm">Back to the Society</Button>
        </Stack>
      </Box>
    </Container>
  );
}

function HostCaseFiles() {
  const { data: cases, error, mutate: refreshCases } = useApiData<CaseSummary[]>(MysteryApi.list);
  const { data: view } = useSociety();
  const atTableId = view?.society.atTable?.mysteryId;

  return (
    <Container size="lg">
      <PageTitle kicker="Host only" title="The Case Files">
        Mysteries written for the Society by the engine. The list keeps its secrets; a case file itself is all spoilers.
      </PageTitle>
      <Group justify="center" mb="lg">
        <Button component={Link} href="/mysteries/lab" variant="light" color="gold" leftSection={<IconFlask size={16} />}>
          The Mystery Lab: write many, export them for review
        </Button>
      </Group>

      {cases && (
        <NextCase
          atTableTitle={view?.society.atTable?.title}
          ready={cases.filter((c) => c.passed && c.status === "generated" && c.mysteryId !== atTableId)}
        />
      )}

      <GeneratePanel onWritten={() => void refreshCases()} />

      <Text className="nx-kicker" ta="center" mt={48} mb="sm">
        Written so far{cases ? ` · ${cases.length}` : ""}
      </Text>
      {error && <ErrorNote message={error.message} />}
      {!cases && !error && <Group justify="center"><Loader color="gold" type="dots" /></Group>}
      {cases?.length === 0 && (
        <Text ta="center" className="nx-flourish" c="var(--nx-sepia)">No mystery has been written yet.</Text>
      )}
      <SimpleGrid cols={{ base: 1, sm: 2, md: 3 }} spacing="lg">
        {cases?.map((summary) => <CaseCard key={summary.mysteryId} summary={summary} atTable={summary.mysteryId === atTableId} />)}
      </SimpleGrid>
    </Container>
  );
}

/**
 * What the Society plays next: the case at the table, and the cases written and checked but not yet played. When a
 * mystery is closed, the next one is written in the background, so one is usually ready.
 */
function NextCase({ atTableTitle, ready }: { atTableTitle?: string; ready: CaseSummary[] }) {
  return (
    <Box className="nx-panel" p="md" maw={760} mx="auto" mb="lg">
      <Stack gap={6} align="center" ta="center">
        {atTableTitle && <Text fz="sm">At the table: <b>{atTableTitle}</b>.</Text>}
        {ready.length > 0 ? (
          <>
            <Text fz="sm">
              Ready to play: {ready.length} case{ready.length === 1 ? "" : "s"}. Next:{" "}
              <Anchor component={Link} href={`/mysteries/${ready[0].mysteryId}`} c="gold.4">{ready[0].title || "Untitled"}</Anchor>
            </Text>
            {!atTableTitle && <TableButton mysteryId={ready[0].mysteryId} playable />}
          </>
        ) : (
          <Text fz="sm" c="var(--nx-sepia)">
            No case is ready to play yet. Write one below; once a mystery is closed, the next one is written by itself.
          </Text>
        )}
      </Stack>
    </Box>
  );
}

/** One case on the shelf: no creature, no truth, just enough to tell them apart. */
export function CaseCard({ summary, atTable = false, href }: { summary: CaseSummary; atTable?: boolean; href?: string }) {
  return (
    <Box
      component={Link}
      href={href ?? `/mysteries/${summary.mysteryId}`}
      className="nx-parchment"
      p="lg"
      style={{ textDecoration: "none", display: "block", minHeight: 150 }}
    >
      <Stack gap={6} h="100%">
        <Group justify="space-between" wrap="nowrap" align="flex-start">
          <IconScript size={22} color="var(--nx-blood)" stroke={1.5} />
          <Group gap={4}>
            {atTable && <Badge size="xs" color="gold" leftSection={<IconCandle size={10} />}>At the table</Badge>}
            <StatusBadge status={summary.passed ? summary.status : "rejected"} />
          </Group>
        </Group>
        <Title order={4}>{summary.title || "Untitled"}</Title>
        <Text fz="sm" c="var(--nx-ink-soft)">
          {levelName(summary.levelId)} · {InWorldDate(summary.month, summary.year)}
        </Text>
        <Text fz="xs" c="var(--nx-ink-soft)" mt="auto">
          Written {new Date(summary.generatedAt).toLocaleString(undefined, { dateStyle: "medium", timeStyle: "short" })}
        </Text>
      </Stack>
    </Box>
  );
}

const StatusLabels: Record<CaseStatus, { label: string; color: string; variant: "filled" | "outline" }> = {
  generated: { label: "Ready to play", color: "moss", variant: "filled" },
  solved: { label: "Solved", color: "gold.8", variant: "filled" },
  failed: { label: "Unresolved", color: "blood", variant: "filled" },
  rejected: { label: "Failed the checks", color: "blood", variant: "outline" },
};

export function StatusBadge({ status }: { status: CaseStatus }) {
  const { label, color, variant } = StatusLabels[status] ?? StatusLabels.generated;
  return <Badge size="xs" color={color} variant={variant}>{label}</Badge>;
}

/** Starts a generation and follows it: the engine's steps, the latest news from it, and how it ended. */
function GeneratePanel({ onWritten }: { onWritten: () => void }) {
  const call = useApi();
  const [starting, setStarting] = useState(false);
  const [startError, setStartError] = useState<string>();
  const { data: job, mutate } = useApiData<MysteryJob | null>(MysteryApi.currentJob, {
    refreshInterval: (current) => (current?.status === "running" ? 2000 : 0),
  });
  const running = job?.status === "running";

  // Refresh the shelf once a generation ends.
  const lastStatus = useRef(job?.status);
  useEffect(() => {
    if (lastStatus.current === "running" && job && job.status !== "running") {
      onWritten();
    }
    lastStatus.current = job?.status;
  }, [job, onWritten]);

  const start = async () => {
    setStarting(true);
    setStartError(undefined);
    const result = await call<MysteryJob>(Http.POST, MysteryApi.generate);
    setStarting(false);
    if (!result.success) {
      setStartError(result.errorMessage);
      return;
    }
    await mutate(result.result ?? null, { revalidate: false });
  };

  return (
    <Box className="nx-panel" p="xl" maw={760} mx="auto">
      <Corners />
      {running && job.kind === "lab" ? (
        <Stack align="center" gap="xs" ta="center">
          <Loader color="gold" type="dots" />
          <Text>The lab is writing case {Math.min(job.results.length + 1, job.total ?? 1)} of {job.total}; the engine takes one job at a time.</Text>
          <Button component={Link} href="/mysteries/lab" size="compact-sm" variant="light" color="gold">Open the lab</Button>
        </Stack>
      ) : running ? (
        <Progress job={job} />
      ) : (
        <Stack align="center" gap="sm" ta="center">
          <SightMark size={22} />
          <Title order={3}>A new mystery</Title>
          <Text className="nx-flourish" c="var(--nx-parchment-2)" maw={560}>
            The engine picks a place and a creature the Society has not met lately, then writes the conflict, what has
            happened, the people and the clues, and a critic reads it over. It takes some minutes; you may leave the page.
          </Text>
          <Button size="md" color="blood" leftSection={<IconFeather size={18} />} onClick={start} loading={starting} mt="xs">
            Write a new mystery
          </Button>
          <ErrorNote message={startError} />
          {job && <LastJob job={job} />}
        </Stack>
      )}
    </Box>
  );
}

export function Progress({ job }: { job: MysteryJob }) {
  const elapsed = useElapsed(job.startedAt);
  const reached = (stepId: string) => job.progress.some((p) => p.startsWith(`${stepId}:`));
  // Seeding the world shows only the first time; a seeding job shows nothing else.
  const steps = job.kind === "world"
    ? GenerationSteps.filter((s) => s.id === "world")
    : GenerationSteps.filter((s) => s.id !== "world" || reached("world"));
  const current = [...steps].reverse().find((s) => reached(s.id))?.id;
  const latest = job.progress.at(-1)?.replace(/^[a-z]+:\s*/, "");
  const attempts = job.progress.filter((p) => p.startsWith("assignment:")).length;

  return (
    <Stack gap="md">
      <Group justify="space-between">
        <Title order={3}>{job.kind === "world" ? "Seeding the Society's world…" : "Writing a mystery…"}</Title>
        <Text fz="sm" c="var(--nx-sepia)" className="nx-flourish">{elapsed}</Text>
      </Group>
      <Stack gap={8}>
        {steps.map((step) => {
          const isCurrent = step.id === current;
          const done = reached(step.id) && !isCurrent;
          return (
            <Group key={step.id} gap="sm" wrap="nowrap" opacity={done || isCurrent ? 1 : 0.45}>
              <ThemeIcon size={22} radius="xl" variant={done ? "filled" : "outline"} color={done ? "moss" : "gold"}>
                {done ? <IconCheck size={14} /> : isCurrent ? <Loader size={12} color="gold" /> : <span />}
              </ThemeIcon>
              <Text fz="sm" fw={isCurrent ? 700 : 400}>{step.label}</Text>
            </Group>
          );
        })}
      </Stack>
      {latest && <Text className="nx-flourish" c="var(--nx-parchment-2)">{latest}</Text>}
      {attempts > 1 && <Text fz="xs" c="var(--nx-sepia)">The first draft did not hold together; this is attempt {attempts}.</Text>}
    </Stack>
  );
}

function LastJob({ job }: { job: MysteryJob }) {
  if (job.kind === "lab") {
    return (
      <Group gap="xs" mt="sm" justify="center">
        <IconFlask size={16} color="var(--nx-gold)" />
        <Text fz="sm">The last lab run: {job.title ?? job.error ?? "ended"}.</Text>
        <Button component={Link} href="/mysteries/lab" size="compact-sm" variant="light" color="gold">Open the lab</Button>
      </Group>
    );
  }
  if (job.kind === "world" && job.status === "succeeded") {
    return (
      <Group gap="xs" mt="sm" justify="center">
        <IconCheck size={16} color="var(--nx-gold)" />
        <Text fz="sm">The Society's world is seeded: {job.title}.</Text>
        <Button component={Link} href="/world" size="compact-sm" variant="light" color="gold">Meet them</Button>
      </Group>
    );
  }
  if (job.status === "succeeded" && job.mysteryId) {
    return (
      <Group gap="xs" mt="sm" justify="center">
        <IconCheck size={16} color="var(--nx-gold)" />
        <Text fz="sm">“{job.title}” is written.</Text>
        <Button component={Link} href={`/mysteries/${job.mysteryId}`} size="compact-sm" variant="light" color="gold">Open the case file</Button>
      </Group>
    );
  }
  if (job.status === "failed") {
    return (
      <Stack gap={4} mt="sm" align="center" maw={620}>
        <Group gap={6}>
          <IconAlertTriangle size={16} color="var(--nx-blood-bright)" />
          <Text fz="sm" c="blood.3">The last attempt failed.</Text>
        </Group>
        <Text fz="xs" c="var(--nx-parchment-2)" style={{ wordBreak: "break-word" }}>{job.error}</Text>
        {job.mysteryId && (
          <Button component={Link} href={`/mysteries/${job.mysteryId}`} size="compact-xs" variant="subtle" color="gold">See what it wrote</Button>
        )}
      </Stack>
    );
  }
  return null;
}

/** "3 min 12 s" since <paramref name="since"/>, ticking each second. */
export function useElapsed(since: string) {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    const timer = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(timer);
  }, []);
  const seconds = Math.max(0, Math.round((now - new Date(since).getTime()) / 1000));
  return seconds < 60 ? `${seconds} s` : `${Math.floor(seconds / 60)} min ${seconds % 60} s`;
}
