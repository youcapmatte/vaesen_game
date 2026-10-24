"use client";

import { Anchor, Badge, Box, Button, Checkbox, Container, CopyButton, Group, Loader, SegmentedControl, Stack, Table, Text, Title } from "@mantine/core";
import { IconCheck, IconCopy, IconDownload, IconFileCode, IconFlask, IconPlayerStop, IconX } from "@tabler/icons-react";
import Link from "next/link";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  LabMaxCount,
  levelName,
  InWorldDate,
  MysteryApi,
  type CaseSummary,
  type ExportFormat,
  type MysteryExport,
  type MysteryJob,
} from "@/common/api/mystery";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { saveFile } from "@/common/client/core/saveFile";
import { useMe } from "@/common/client/components/hooks";
import { Corners } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame, PageTitle } from "@/common/client/components/PageFrame";
import { HostOnly, Progress, StatusBadge, useElapsed } from "./CaseFilesPage";

/** What to ask Claude when handing it an export. */
const ReviewPrompt = `These are mysteries generated for Vaesen, a Nordic horror tabletop game, by an AI mystery engine. For each case, assess:
1. Coherence: does the truth hold together, and can the players actually reach it through the clues?
2. Fairness: is every load-bearing fact reachable by more than one route, and are the red herrings fair?
3. Atmosphere: does it feel like Vaesen (folklore, 19th-century Scandinavia, slow dread), not generic horror?
4. Playability: could a Gamemaster run it by voice for 2 to 4 players in one or two evenings?
5. People: do the non-player characters want things, lie for reasons and feel distinct?
Score each case 1 to 5 on each point with a sentence of why, then list what the engine gets wrong across cases and what to change in its prompts or checks.`;

/** The host's lab: write mysteries one after another, keep them apart from the Society's own, and export them for review. */
export function LabPage() {
  return (
    <PageFrame>
      <LabContent />
    </PageFrame>
  );
}

function LabContent() {
  const { data: me } = useMe();
  if (!me) {
    return <FullPageLoader label="Lighting the lamps in the lab…" />;
  }
  return me.role === "Host" ? <Lab /> : <HostOnly />;
}

function Lab() {
  const [shelf, setShelf] = useState<"lab" | "society">("lab");
  const { data: cases, error, mutate: refreshCases } = useApiData<CaseSummary[]>(shelf === "lab" ? MysteryApi.labList : MysteryApi.list);

  return (
    <Container size="lg">
      <PageTitle kicker="Host only" title="The Mystery Lab">
        Write mysteries one after another to see what the engine makes. Lab cases are kept apart from the Society&apos;s own
        and never count as played, so each one is a fresh sample of the next mystery. Export them to have Claude assess them.
      </PageTitle>

      <RunPanel onCaseWritten={() => void refreshCases()} />

      <Group justify="space-between" mt={48} mb="sm">
        <Text className="nx-kicker">{shelf === "lab" ? "Written in the lab" : "The Society's case files"}{cases ? ` · ${cases.length}` : ""}</Text>
        <SegmentedControl
          size="xs"
          color="gold"
          value={shelf}
          onChange={(value) => setShelf(value as "lab" | "society")}
          data={[{ value: "lab", label: "Lab" }, { value: "society", label: "Society" }]}
        />
      </Group>
      {error && <ErrorNote message={error.message} />}
      {!cases && !error && <Group justify="center"><Loader color="gold" type="dots" /></Group>}
      {cases && <CaseTable key={shelf} cases={cases} />}
    </Container>
  );
}

/** Starts a run of N cases and follows it: which case it is on, that case's steps, and how each finished one went. */
function RunPanel({ onCaseWritten }: { onCaseWritten: () => void }) {
  const call = useApi();
  const [count, setCount] = useState("3");
  const [starting, setStarting] = useState(false);
  const [startError, setStartError] = useState<string>();
  const { data: job, mutate } = useApiData<MysteryJob | null>(MysteryApi.currentJob, {
    refreshInterval: (current) => (current?.status === "running" ? 2000 : 0),
  });
  const lab = job?.kind === "lab" ? job : null;
  const running = job?.status === "running";

  // Refresh the shelf as each case lands.
  const written = lab?.results.length ?? 0;
  const lastWritten = useRef(written);
  useEffect(() => {
    if (written !== lastWritten.current) {
      onCaseWritten();
    }
    lastWritten.current = written;
  }, [written, onCaseWritten]);

  const start = async () => {
    setStarting(true);
    setStartError(undefined);
    const result = await call<MysteryJob>(Http.POST, MysteryApi.lab, { count: Number(count) });
    setStarting(false);
    if (!result.success) {
      setStartError(result.errorMessage);
      return;
    }
    await mutate(result.result ?? null, { revalidate: false });
  };

  const stop = async () => {
    if (!lab) {
      return;
    }
    const result = await call<MysteryJob>(Http.POST, MysteryApi.stop(lab.jobId));
    if (result.success) {
      await mutate(result.result ?? null, { revalidate: false });
    }
  };

  return (
    <Box className="nx-panel" p="xl" maw={760} mx="auto">
      <Corners />
      {running && lab ? (
        <RunningLab job={lab} onStop={stop} />
      ) : running ? (
        <Stack align="center" gap="xs" ta="center">
          <Loader color="gold" type="dots" />
          <Text>The engine is writing a case for the Society; the lab can start when it is done.</Text>
          <Button component={Link} href="/mysteries" size="compact-sm" variant="light" color="gold">The Case Files</Button>
        </Stack>
      ) : (
        <Stack align="center" gap="sm" ta="center">
          <IconFlask size={26} color="var(--nx-gold)" />
          <Title order={3}>A run of mysteries</Title>
          <Text className="nx-flourish" c="var(--nx-parchment-2)" maw={560}>
            Each case takes some minutes and many large model calls, so a long run costs real money. You may leave the
            page; the run carries on, and you can stop it after the case it is writing.
          </Text>
          <Group gap="sm" mt="xs">
            <SegmentedControl
              color="gold"
              value={count}
              onChange={setCount}
              data={["1", "3", "5", String(LabMaxCount)].map((n) => ({ value: n, label: `${n} case${n === "1" ? "" : "s"}` }))}
            />
            <Button color="blood" leftSection={<IconFlask size={16} />} onClick={start} loading={starting}>Start the run</Button>
          </Group>
          <ErrorNote message={startError} />
          {lab && <Results job={lab} />}
        </Stack>
      )}
    </Box>
  );
}

function RunningLab({ job, onStop }: { job: MysteryJob; onStop: () => void }) {
  const elapsed = useElapsed(job.startedAt);
  // The engine's steps for the case being written: the progress since the run said which case it is on.
  const current = useMemo(() => {
    const start = job.progress.findLastIndex((p) => p.startsWith("lab:"));
    return { ...job, kind: "mystery" as const, progress: job.progress.slice(start + 1) };
  }, [job]);

  return (
    <Stack gap="md">
      <Group justify="space-between">
        <Text className="nx-kicker">Case {Math.min(job.results.length + 1, job.total ?? 1)} of {job.total} · {elapsed} in all</Text>
        <Button
          size="compact-sm"
          variant="subtle"
          color="umber"
          leftSection={<IconPlayerStop size={14} />}
          onClick={onStop}
          disabled={job.stopRequested}
        >
          {job.stopRequested ? "Stopping after this case" : "Stop after this case"}
        </Button>
      </Group>
      <Progress job={current} />
      {job.results.length > 0 && <Results job={job} />}
    </Stack>
  );
}

function Results({ job }: { job: MysteryJob }) {
  return (
    <Stack gap={4} mt="sm" w="100%">
      {job.title && job.status !== "running" && <Text fz="sm" fw={700}>The last run: {job.title}.</Text>}
      {job.error && <ErrorNote message={job.error} />}
      {job.results.map((result, i) => (
        <Group key={i} gap="xs" wrap="nowrap" justify="center">
          {result.passed ? <IconCheck size={14} color="var(--nx-moss)" /> : <IconX size={14} color="var(--nx-blood-bright)" />}
          {result.mysteryId
            ? <Anchor component={Link} href={`/mysteries/${result.mysteryId}`} fz="sm" c="gold.4">{result.title || "Untitled"}</Anchor>
            : <Text fz="sm">{result.title || "Nothing kept"}</Text>}
          {!result.passed && result.error && <Text fz="xs" c="var(--nx-sepia)" lineClamp={1}>{result.error}</Text>}
        </Group>
      ))}
    </Stack>
  );
}

/** The cases with checkboxes, and the export of the chosen ones. */
function CaseTable({ cases }: { cases: CaseSummary[] }) {
  const call = useApi();
  const [chosen, setChosen] = useState<string[]>([]);
  const [exporting, setExporting] = useState<ExportFormat | null>(null);
  const [exportError, setExportError] = useState<string>();
  const all = cases.length > 0 && chosen.length === cases.length;

  const toggle = (id: string) => setChosen((current) => (current.includes(id) ? current.filter((x) => x !== id) : [...current, id]));

  const exportCases = async (format: ExportFormat) => {
    setExporting(format);
    setExportError(undefined);
    const result = await call<MysteryExport>(Http.POST, MysteryApi.export, { mysteryIds: chosen, format });
    setExporting(null);
    if (!result.success || !result.result) {
      setExportError(result.errorMessage);
      return;
    }
    saveFile(result.result.fileName, result.result.content, result.result.contentType);
  };

  if (cases.length === 0) {
    return <Text ta="center" className="nx-flourish" c="var(--nx-sepia)">Nothing written here yet.</Text>;
  }

  return (
    <Stack gap="md">
      <Box className="nx-parchment" p="md" style={{ overflowX: "auto" }}>
        <Table fz="sm" highlightOnHover verticalSpacing={6} miw={560}>
          <Table.Thead>
            <Table.Tr>
              <Table.Th w={36}>
                <Checkbox
                  size="xs"
                  color="blood"
                  aria-label="Choose every case"
                  checked={all}
                  indeterminate={chosen.length > 0 && !all}
                  onChange={() => setChosen(all ? [] : cases.map((c) => c.mysteryId))}
                />
              </Table.Th>
              <Table.Th>Case</Table.Th>
              <Table.Th>Place</Table.Th>
              <Table.Th>Checks</Table.Th>
              <Table.Th>Written</Table.Th>
            </Table.Tr>
          </Table.Thead>
          <Table.Tbody>
            {cases.map((c) => (
              <Table.Tr key={c.mysteryId}>
                <Table.Td>
                  <Checkbox size="xs" color="blood" aria-label={`Choose ${c.title}`} checked={chosen.includes(c.mysteryId)} onChange={() => toggle(c.mysteryId)} />
                </Table.Td>
                <Table.Td>
                  <Anchor component={Link} href={`/mysteries/${c.mysteryId}`} c="var(--nx-ink)" fw={600}>{c.title || "Untitled"}</Anchor>
                </Table.Td>
                <Table.Td>{levelName(c.levelId)} · {InWorldDate(c.month, c.year)}</Table.Td>
                <Table.Td><StatusBadge status={c.passed ? c.status : "rejected"} /></Table.Td>
                <Table.Td c="var(--nx-ink-soft)">{new Date(c.generatedAt).toLocaleString(undefined, { dateStyle: "short", timeStyle: "short" })}</Table.Td>
              </Table.Tr>
            ))}
          </Table.Tbody>
        </Table>
      </Box>

      <Box className="nx-panel" p="lg">
        <Stack gap="sm">
          <Group justify="space-between" wrap="wrap" gap="sm">
            <Text fw={700}>{chosen.length === 0 ? "Choose cases to export" : `${chosen.length} chosen`}</Text>
            <Group gap="xs">
              <Button
                size="xs"
                color="gold"
                leftSection={<IconDownload size={14} />}
                disabled={chosen.length === 0}
                loading={exporting === "markdown"}
                onClick={() => exportCases("markdown")}
              >
                Export for review (Markdown)
              </Button>
              <Button
                size="xs"
                variant="outline"
                color="gold"
                leftSection={<IconFileCode size={14} />}
                disabled={chosen.length === 0}
                loading={exporting === "json"}
                onClick={() => exportCases("json")}
              >
                Export everything (JSON)
              </Button>
            </Group>
          </Group>
          <ErrorNote message={exportError} />
          <Text fz="sm" c="var(--nx-parchment-2)">
            The Markdown file has each case&apos;s full report: truth, cast, clues, the checks and how it was written. Give it
            to Claude with a request like this one:
          </Text>
          <Box p="sm" style={{ background: "rgba(0,0,0,0.25)", borderRadius: 6 }}>
            <Text fz="xs" style={{ whiteSpace: "pre-wrap" }} c="var(--nx-parchment-2)">{ReviewPrompt}</Text>
          </Box>
          <Group>
            <CopyButton value={ReviewPrompt}>
              {({ copied, copy }) => (
                <Button size="compact-xs" variant="subtle" color="gold" leftSection={copied ? <IconCheck size={12} /> : <IconCopy size={12} />} onClick={copy}>
                  {copied ? "Copied" : "Copy the request"}
                </Button>
              )}
            </CopyButton>
            <Badge variant="outline" color="gold" size="sm">Cases are spoilers: don&apos;t review one you mean to play</Badge>
          </Group>
        </Stack>
      </Box>
    </Stack>
  );
}
