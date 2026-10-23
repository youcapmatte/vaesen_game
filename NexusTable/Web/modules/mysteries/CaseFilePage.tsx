"use client";

import { Accordion, Badge, Box, Button, Container, Group, Modal, MultiSelect, Progress, SegmentedControl, Select, SimpleGrid, Stack, Table, Tabs, Text, Textarea, Title } from "@mantine/core";
import { useDisclosure } from "@mantine/hooks";
import { IconAlertTriangle, IconArrowLeft, IconBook2, IconDownload, IconEye, IconFileCode } from "@tabler/icons-react";
import Link from "next/link";
import { useState } from "react";
import {
  FocusNames,
  ReasonNames,
  levelName,
  MonthNames,
  MysteryApi,
  type CaseChannel,
  type CaseFact,
  type CasePacket,
  type CloseMysteryRequest,
  type CastMember,
  type CoherenceIssue,
  type StoredCase,
} from "@/common/api/mystery";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { useMe } from "@/common/client/components/hooks";
import { Corners } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame } from "@/common/client/components/PageFrame";
import { saveFile } from "@/common/client/core/saveFile";
import { HostOnly, StatusBadge } from "./CaseFilesPage";
import { TableButton } from "./TableButton";

/** One case file: the hidden GM file of a mystery, behind a spoiler warning. Host only. */
export function CaseFilePage({ mysteryId }: { mysteryId: string }) {
  return (
    <PageFrame>
      <CaseFileContent mysteryId={mysteryId} />
    </PageFrame>
  );
}

function CaseFileContent({ mysteryId }: { mysteryId: string }) {
  const { data: me } = useMe();
  const isHost = me?.role === "Host";
  const { data: stored, error, mutate } = useApiData<StoredCase>(isHost ? MysteryApi.case(mysteryId) : null);
  const [revealed, setRevealed] = useState(false);

  if (!me) {
    return <FullPageLoader label="Finding the key to the archive…" />;
  }
  if (!isHost) {
    return <HostOnly />;
  }

  return (
    <Container size="lg">
      <Group mb="md">
        <Button component={Link} href="/mysteries" variant="subtle" color="gold" leftSection={<IconArrowLeft size={16} />}>The Case Files</Button>
      </Group>
      {error && <ErrorNote message={error.message} />}
      {!stored && !error && <FullPageLoader label="Breaking the seal…" />}
      {stored && (revealed
        ? <CaseFile stored={stored} onChanged={(next) => void mutate(next, { revalidate: false })} />
        : <SpoilerGate packet={stored.packet} playable={stored.packet.validation.passed && !stored.closing} onReveal={() => setRevealed(true)} />)}
    </Container>
  );
}

function SpoilerGate({ packet, playable, onReveal }: { packet: CasePacket; playable: boolean; onReveal: () => void }) {
  return (
    <Box className="nx-parchment" p="xl" maw={640} mx="auto" ta="center">
      <Corners color="var(--nx-blood)" />
      <Stack align="center" gap="sm">
        <IconAlertTriangle size={30} color="var(--nx-blood)" />
        <Title order={2}>{packet.title || "Untitled"}</Title>
        <Text fz="sm" c="var(--nx-ink-soft)">{levelName(packet.brief.levelId)} · {MonthNames[packet.brief.month - 1]}</Text>
        <Text className="nx-flourish" maw={480}>
          This is the hidden case file: the creature, the truth, who lies and where every clue lies. If you mean to play
          this mystery yourself, close it now.
        </Text>
        <Group mt="sm">
          <Button component={Link} href="/mysteries" variant="subtle" color="umber">Keep it sealed</Button>
          <Button color="blood" leftSection={<IconEye size={16} />} onClick={onReveal}>Break the seal</Button>
        </Group>
        {playable && (
          <Stack gap={4} align="center" mt="md">
            <Text fz="xs" c="var(--nx-ink-soft)">Or play it unread: the table sees only its title and hook.</Text>
            <TableButton mysteryId={packet.mysteryId} playable />
          </Stack>
        )}
      </Stack>
    </Box>
  );
}

function CaseFile({ stored, onChanged }: { stored: StoredCase; onChanged: (next: StoredCase) => void }) {
  const p = stored.packet;
  const [closing, { open: openClose, close: closeClose }] = useDisclosure(false);
  const canClose = p.validation.passed && !stored.closing;
  const errors = p.validation.issues.filter((i) => i.severity === "error").length;
  const warnings = p.validation.issues.length - errors;

  return (
    <Stack gap="lg">
      <Stack gap={4} align="center" ta="center">
        <Text className="nx-kicker">Case file · {levelName(p.brief.levelId)} · {MonthNames[p.brief.month - 1]}</Text>
        <Title order={1}>{p.title || "Untitled"}</Title>
        <Group gap={6} justify="center" mt={4}>
          {p.brief.vaesenIds.map((v) => <Badge key={v} color="blood">{v}</Badge>)}
          <Badge variant="outline" color="gold">{FocusNames[p.truth.focus] ?? p.truth.focus}</Badge>
          <Badge variant="outline" color="gold">{ReasonNames[p.truth.reasonKind] ?? p.truth.reasonKind}</Badge>
          {p.truth.plotCentre.kind === "hiddenHand" && <Badge variant="outline" color="blood.4">Hidden hand</Badge>}
          {p.validation.passed
            ? <Badge color="moss" variant="light">Passed · {warnings} warning{warnings === 1 ? "" : "s"}</Badge>
            : <Badge color="blood" variant="light">Failed · {errors} error{errors === 1 ? "" : "s"}</Badge>}
          <StatusBadge status={p.validation.passed ? stored.status : "rejected"} />
        </Group>
        {stored.failedStep && (
          <Text fz="sm" c="blood.3" mt={4}>
            The {stored.failedStep} step gave up: the case stops there. Its last attempt is in the review tab.
          </Text>
        )}
        <Box mt="sm"><TableButton mysteryId={p.mysteryId} playable={canClose} /></Box>
        {canClose && (
          <Button mt="sm" size="xs" variant="outline" color="gold" leftSection={<IconBook2 size={14} />} onClick={openClose}>
            The Society has finished this mystery
          </Button>
        )}
      </Stack>

      {stored.closing && <ClosingSummary stored={stored} />}
      <CloseModal opened={closing} onClose={closeClose} stored={stored} onClosed={onChanged} />

      <Tabs defaultValue="truth" color="gold" variant="outline">
        <Tabs.List grow>
          <Tabs.Tab value="truth">The truth</Tabs.Tab>
          <Tabs.Tab value="hook">Hook & opening</Tabs.Tab>
          <Tabs.Tab value="clues">Clues · {p.facts.length}</Tabs.Tab>
          <Tabs.Tab value="people">People · {p.cast.length}</Tabs.Tab>
          <Tabs.Tab value="endings">Beats & endings</Tabs.Tab>
          <Tabs.Tab value="review">Review</Tabs.Tab>
        </Tabs.List>
        <Box pt="lg">
          <Tabs.Panel value="truth"><TruthTab p={p} /></Tabs.Panel>
          <Tabs.Panel value="hook"><HookTab p={p} /></Tabs.Panel>
          <Tabs.Panel value="clues"><CluesTab p={p} /></Tabs.Panel>
          <Tabs.Panel value="people"><PeopleTab p={p} /></Tabs.Panel>
          <Tabs.Panel value="endings"><EndingsTab p={p} /></Tabs.Panel>
          <Tabs.Panel value="review"><ReviewTab stored={stored} /></Tabs.Panel>
        </Box>
      </Tabs>
    </Stack>
  );
}

function Sheet({ title, children }: { title?: string; children: React.ReactNode }) {
  return (
    <Box className="nx-parchment" p="xl">
      {title && <Title order={3} mb="sm">{title}</Title>}
      {children}
    </Box>
  );
}

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <Box>
      <Text className="nx-kicker" c="var(--nx-blood)" fz={11}>{label}</Text>
      <Text fz="sm">{children}</Text>
    </Box>
  );
}

function TruthTab({ p }: { p: CasePacket }) {
  const hand = p.cast.find((c) => c.characterId === p.truth.plotCentre.characterId);
  return (
    <Sheet>
      <Text className="nx-dropcap" style={{ whiteSpace: "pre-wrap" }}>{p.truth.summary}</Text>
      <SimpleGrid cols={{ base: 1, sm: 2 }} mt="lg" spacing="md">
        <Field label={`The provocation · ${p.truth.provocationPlaceId}`}>{p.truth.provocation}</Field>
        {p.truth.hiddenHandMotive && <Field label={`The hidden hand${hand ? ` · ${hand.name}` : ""}`}>{p.truth.hiddenHandMotive}</Field>}
        {p.truth.decoyReading && <Field label="What folk will think">{p.truth.decoyReading}</Field>}
        {p.truth.origin && <Field label="Origin">{p.truth.origin}</Field>}
        <Field label="Conflict">{p.brief.conflictArchetypeId}</Field>
      </SimpleGrid>
    </Sheet>
  );
}

function HookTab({ p }: { p: CasePacket }) {
  const giver = p.cast.find((c) => c.characterId === p.hook.hookGiverId);
  return (
    <Stack gap="lg">
      <Sheet title={`The hook · ${p.hook.channel}${giver ? `, from ${giver.name}` : ""}`}>
        <Text className="nx-flourish" fz="lg" style={{ whiteSpace: "pre-wrap" }}>{p.hook.text}</Text>
      </Sheet>
      <Sheet title="What has happened">
        <Stack gap="sm">
          {p.truth.escalation.map((e) => (
            <Group key={e.id} gap="sm" align="flex-start" wrap="nowrap">
              <Badge size="sm" variant={e.opening ? "filled" : "outline"} color={e.opening ? "blood" : "umber"} miw={64}>{e.opening ? "opening" : e.id}</Badge>
              <Box>
                <Text fz="sm">{e.description}</Text>
                <Text fz="xs" c="var(--nx-ink-soft)">{e.when} · {e.placeId ?? "out in the land"} · {e.register}</Text>
              </Box>
            </Group>
          ))}
        </Stack>
      </Sheet>
      {Object.keys(p.level.sceneDescriptions).length > 0 && (
        <Sheet title="Scenes">
          <Stack gap="sm">
            {Object.entries(p.level.sceneDescriptions).map(([place, text]) => <Field key={place} label={place}>{text}</Field>)}
          </Stack>
        </Sheet>
      )}
    </Stack>
  );
}

function CluesTab({ p }: { p: CasePacket }) {
  const name = (id?: string | null) => p.cast.find((c) => c.characterId === id)?.name ?? id ?? "";
  const facts = [...p.facts].sort((a, b) => Number(b.loadBearing) - Number(a.loadBearing));
  return (
    <Sheet>
      <Text fz="sm" c="var(--nx-ink-soft)" mb="md">
        Every fact of the case and each way in. Load-bearing facts come first; the case cannot be solved without them.
      </Text>
      <Accordion variant="separated" multiple styles={{ item: { background: "rgba(255,255,255,0.25)", borderColor: "rgba(43,33,24,0.2)" } }}>
        {facts.map((fact) => (
          <Accordion.Item key={fact.id} value={fact.id}>
            <Accordion.Control>
              <Group gap="xs" wrap="nowrap" align="flex-start">
                <Text fz="xs" c="var(--nx-ink-soft)" miw={28}>{fact.id}</Text>
                <Box style={{ flex: 1 }}>
                  <Text fz="sm" c="var(--nx-ink)">{fact.statement}</Text>
                  <FactBadges fact={fact} channels={p.channels.filter((c) => c.factId === fact.id).length} />
                </Box>
              </Group>
            </Accordion.Control>
            <Accordion.Panel>
              <Stack gap="xs">
                {p.channels.filter((c) => c.factId === fact.id).map((c) => <ChannelLine key={c.id} channel={c} via={c.sourceCharacterId ? name(c.sourceCharacterId) : `${c.placeId ?? ""}${c.affordanceId ? ` / ${c.affordanceId}` : ""}`} />)}
                {p.channels.every((c) => c.factId !== fact.id) && <Text fz="xs" c="var(--nx-ink-soft)">No channel: known from the hook, or put into the world by a beat.</Text>}
              </Stack>
            </Accordion.Panel>
          </Accordion.Item>
        ))}
      </Accordion>
    </Sheet>
  );
}

function FactBadges({ fact, channels }: { fact: CaseFact; channels: number }) {
  const decoy = fact.pointsTo.startsWith("decoy:");
  return (
    <Group gap={4} mt={4}>
      {fact.loadBearing && <Badge size="xs" color="blood">load-bearing</Badge>}
      <Badge size="xs" variant="outline" color="umber">{fact.kind}</Badge>
      {decoy && <Badge size="xs" color="gold.8">{fact.pointsTo}</Badge>}
      {fact.deepLore && <Badge size="xs" color="moss">deep lore</Badge>}
      {fact.giveaway && <Badge size="xs" variant="outline" color="blood">giveaway</Badge>}
      <Text fz={11} c="var(--nx-ink-soft)">{channels === 0 ? "known from the start" : `${channels} way${channels === 1 ? "" : "s"} in`}</Text>
    </Group>
  );
}

function ChannelLine({ channel, via }: { channel: CaseChannel; via: string }) {
  return (
    <Box pl="sm" style={{ borderLeft: "2px solid var(--nx-gold-dim)" }}>
      <Group gap={6}>
        <Badge size="xs" color="umber">{channel.kind}</Badge>
        <Text fz="xs" fw={600}>{via}</Text>
        {channel.access.requires.length === 0
          ? <Badge size="xs" variant="outline" color="moss.8">open</Badge>
          : channel.access.requires.map((r) => <Badge key={r} size="xs" variant="outline" color="gold">{r}</Badge>)}
      </Group>
      <Text fz="sm" fs="italic" mt={2}>{channel.reveal}</Text>
    </Box>
  );
}

function PeopleTab({ p }: { p: CasePacket }) {
  const order = (c: CastMember) => (c.importance === "significant" ? 0 : 1);
  return (
    <SimpleGrid cols={{ base: 1, md: 2 }} spacing="lg">
      {[...p.cast].sort((a, b) => order(a) - order(b)).map((c) => (
        <Box key={c.characterId} className="nx-parchment" p="lg">
          <Group justify="space-between" align="flex-start" wrap="nowrap">
            <Box>
              <Title order={4}>{c.name}</Title>
              <Text fz="xs" c="var(--nx-ink-soft)">
                {c.identity ? `${c.identity.age}, ${c.identity.occupation}` : c.characterId}{c.homePlaceId ? ` · ${c.homePlaceId}` : ""}
              </Text>
            </Box>
            <Group gap={4} justify="flex-end">
              <Badge size="xs" color={c.importance === "significant" ? "blood" : "umber"}>{c.plotRole || c.importance}</Badge>
              {c.instanceRef !== "new" && <Badge size="xs" variant="outline" color="gold">{c.instanceRef}</Badge>}
            </Group>
          </Group>
          <Stack gap={6} mt="sm">
            {c.wants.drives.length > 0 && <Field label="Wants">{c.wants.drives.join("; ")}</Field>}
            {c.wants.guilt && <Field label="Guilt">{c.wants.guilt}</Field>}
            {c.secrets.length > 0 && <Field label="Secrets">{c.secrets.join(" ")}</Field>}
            {c.lies.map((l) => <Field key={l.about} label={`Lies about ${l.about}`}>{l.story}</Field>)}
            {c.knows.length > 0 && <Field label="Knows">{c.knows.map((k) => `${k.factId} (${k.source})`).join(", ")}</Field>}
            {c.thinks.map((t) => <Field key={t.belief} label={t.true ? "Thinks, rightly" : "Thinks, wrongly"}>{t.belief}</Field>)}
            {c.voiceNotes && <Field label="Voice">{c.voiceNotes}</Field>}
          </Stack>
        </Box>
      ))}
    </SimpleGrid>
  );
}

function EndingsTab({ p }: { p: CasePacket }) {
  const name = (id: string) => p.cast.find((c) => c.characterId === id)?.name ?? id;
  return (
    <Stack gap="lg">
      <Sheet title="Beats">
        <Stack gap="sm">
          {[...p.beats].sort((a, b) => a.threshold - b.threshold).map((b) => (
            <Box key={b.id}>
              <Group gap="sm" wrap="nowrap">
                <Progress value={b.threshold * 100} w={80} color="blood" size="sm" />
                <Text fz="xs" c="var(--nx-ink-soft)">{b.threshold.toFixed(2)}{b.requires.place ? ` · ${b.requires.place}` : ""}{b.requires.time ? ` · ${b.requires.time}` : ""}</Text>
              </Group>
              <Text fz="sm" mt={2}>{b.description}</Text>
            </Box>
          ))}
          <Box pt="xs" style={{ borderTop: "1px solid rgba(43,33,24,0.2)" }}>
            <Field label={`Climax · at ${p.climax.threshold.toFixed(2)}${p.climax.conditions.place ? `, ${p.climax.conditions.place}` : ""}${p.climax.conditions.time ? `, ${p.climax.conditions.time}` : ""}`}>
              {p.climax.description}
            </Field>
          </Box>
        </Stack>
      </Sheet>
      <Sheet title="How it can end">
        <Stack gap="md">
          {p.resolutionLevers.map((r) => (
            <Box key={r.id}>
              <Group gap={6}><Badge size="xs" color={r.solveDepth === "deep" ? "moss" : r.solveDepth === "partial" ? "gold.8" : "blood"}>{r.solveDepth} solve</Badge><Text fz="xs" c="var(--nx-ink-soft)">needs {r.requiresFactIds.join(", ") || "nothing"}</Text></Group>
              <Text fz="sm" mt={2}>{r.description}</Text>
              {r.requiredDeeds?.map((d) => (
                <Text key={d.id} fz="xs" c="var(--nx-parchment-2)">
                  {d.id}: {d.description}
                  {[d.place, d.time].filter(Boolean).length > 0 && ` (${[d.place, d.time].filter(Boolean).join(", ")})`}
                  {(d.performedBy?.who.length ?? 0) > 0 && `, by ${d.performedBy!.who.length > 1 ? `${d.performedBy!.rule} of ` : ""}${d.performedBy!.who.map((w) => (w === "players" ? "the players" : name(w))).join(", ")}`}
                  {d.witnesses.length > 0 && `, before ${d.witnesses.length > 1 ? `${d.witnessRule} of ` : ""}${d.witnesses.map(name).join(", ")}`}
                </Text>
              ))}
              {r.consequences.world.length > 0 && <Text fz="xs" c="var(--nx-ink-soft)" fs="italic">{r.consequences.world.join(" ")}</Text>}
              {r.consequences.attitudes?.map((a) => (
                <Text key={a.characterId} fz="xs" c="var(--nx-ink-soft)">
                  {name(a.characterId)}: {a.delta > 0 ? `+${a.delta}` : a.delta} · {a.change}
                </Text>
              ))}
              {r.consequences.fates?.map((f) => (
                <Text key={f.characterId} fz="xs" c="var(--nx-blood)">{name(f.characterId)} {f.status === "dead" ? "dies" : "leaves"}: {f.reason}</Text>
              ))}
            </Box>
          ))}
        </Stack>
      </Sheet>
      {p.decoys.length > 0 && (
        <Sheet title="False trails">
          <Stack gap="md">
            {p.decoys.map((d) => (
              <Box key={d.threadId}>
                <Text fz="sm" fw={600}>{d.description}</Text>
                <Text fz="xs" c="var(--nx-ink-soft)">Cleared by {d.clearedByFactIds.join(", ")}. {d.resolution}</Text>
              </Box>
            ))}
          </Stack>
        </Sheet>
      )}
    </Stack>
  );
}

function ReviewTab({ stored }: { stored: StoredCase }) {
  const p = stored.packet;
  const call = useApi();
  const [downloadError, setDownloadError] = useState<string>();
  const issues = [...p.validation.issues].sort((a, b) => Number(a.severity !== "error") - Number(b.severity !== "error"));
  const totals = stored.log.reduce((t, e) => ({ input: t.input + e.inputTokens, cached: t.cached + e.cacheReadTokens, output: t.output + e.outputTokens, ms: t.ms + e.latencyMs }), { input: 0, cached: 0, output: 0, ms: 0 });

  // Everything there is about the case, for Claude or anyone else reviewing the engine: packet, prompts, every step's
  // instruction and raw answer, validator runs and the closing.
  const downloadAll = () => saveFile(`${p.mysteryId}.json`, JSON.stringify(stored, null, 2), "application/json");

  const download = async () => {
    const result = await call<string>(Http.GET, MysteryApi.report(p.mysteryId));
    if (!result.success || !result.result) {
      setDownloadError(result.errorMessage);
      return;
    }
    saveFile(`${p.title || p.mysteryId}.md`, result.result, "text/markdown");
  };

  return (
    <Stack gap="lg">
      {/* First, so they are not buried under a long list of checks. */}
      <Group justify="center">
        <Button color="gold" variant="light" leftSection={<IconDownload size={16} />} onClick={download}>Download the report for Claude (Markdown)</Button>
        <Button color="gold" variant="subtle" leftSection={<IconFileCode size={16} />} onClick={downloadAll}>Download everything (JSON)</Button>
        <ErrorNote message={downloadError} />
      </Group>
      <Sheet title="What the checks found">
        {issues.length === 0 && <Text fz="sm">Nothing: every structural check and the critic passed.</Text>}
        <Stack gap="xs">
          {issues.map((issue, i) => <IssueLine key={i} issue={issue} />)}
        </Stack>
      </Sheet>
      <Sheet title="How it was written">
        <Text fz="xs" c="var(--nx-ink-soft)" mb="xs">
          {p.models.creative}{p.models.critic ? `, critic ${p.models.critic}` : ""} · {totals.input.toLocaleString()} tokens in ({totals.cached.toLocaleString()} cached),
          {" "}{totals.output.toLocaleString()} out · {(totals.ms / 1000).toFixed(0)} s · {p.validation.runs} checks
          {stored.brief.notes.length > 0 ? ` · ${stored.brief.notes.join(" ")}` : ""}
        </Text>
        <Table fz="xs" striped withRowBorders={false}>
          <Table.Thead>
            <Table.Tr><Table.Th>Step</Table.Th><Table.Th>Try</Table.Th><Table.Th>Kind</Table.Th><Table.Th>Result</Table.Th><Table.Th ta="right">Out</Table.Th><Table.Th ta="right">Seconds</Table.Th></Table.Tr>
          </Table.Thead>
          <Table.Tbody>
            {stored.log.map((e, i) => (
              <Table.Tr key={i}>
                <Table.Td>{e.caseAttempt > 0 ? `draft ${e.caseAttempt + 1}: ` : ""}{e.step}</Table.Td>
                <Table.Td>{e.attempt + 1}</Table.Td>
                <Table.Td>{e.kind ?? "full"}</Table.Td>
                <Table.Td>{e.passed ? "passed" : e.error ?? [...new Set(e.issues.filter((x) => x.severity === "error").map((x) => x.code))].join(", ")}</Table.Td>
                <Table.Td ta="right">{e.outputTokens.toLocaleString()}</Table.Td>
                <Table.Td ta="right">{(e.latencyMs / 1000).toFixed(0)}</Table.Td>
              </Table.Tr>
            ))}
          </Table.Tbody>
        </Table>
      </Sheet>
    </Stack>
  );
}

function IssueLine({ issue }: { issue: CoherenceIssue }) {
  const error = issue.severity === "error";
  return (
    <Box pl="sm" style={{ borderLeft: `2px solid ${error ? "var(--nx-blood)" : "var(--nx-gold-dim)"}` }}>
      <Group gap={6}>
        <Badge size="xs" color={error ? "blood" : "gold"} variant={error ? "filled" : "light"}>{issue.severity}</Badge>
        <Text fz="xs" fw={600}>{issue.code}</Text>
        <Text fz="xs" c="var(--nx-ink-soft)">{issue.location} · {issue.source}</Text>
        {issue.applied && <Badge size="xs" color="moss" variant="light">fix applied</Badge>}
      </Group>
      <Text fz="sm">{issue.explanation}</Text>
      {issue.suggestedFix && <Text fz="xs" fs="italic" c="var(--nx-ink-soft)">{issue.suggestedFix}</Text>}
    </Box>
  );
}

/** How the mystery ended and what that did to the Society's world. */
function ClosingSummary({ stored }: { stored: StoredCase }) {
  const c = stored.closing!;
  const lever = stored.packet.resolutionLevers.find((l) => l.id === c.leverId);
  return (
    <Box className="nx-panel" p="lg">
      <Corners />
      <Stack gap={6}>
        <Text className="nx-kicker">
          {c.outcome === "solved" ? "Solved" : "Unresolved"} · {new Date(c.closedAt).toLocaleDateString(undefined, { dateStyle: "medium" })}
          {c.solveDepth ? ` · ${c.solveDepth} solve` : ""}
        </Text>
        {lever && <Text fz="sm">{lever.description}</Text>}
        {c.notes && <Text fz="sm" fs="italic" c="var(--nx-parchment-2)">{c.notes}</Text>}
        {(c.deedsDone?.length ?? 0) > 0 && <Text fz="xs" c="var(--nx-ink-soft)">Deeds done: {c.deedsDone!.join(", ")}</Text>}
        {c.changes.length > 0 && (
          <Box mt={4}>
            <Text fz="xs" c="var(--nx-gold)">What the world remembers</Text>
            {c.changes.map((line, i) => <Text key={i} fz="sm" c="var(--nx-parchment-2)">· {line}</Text>)}
          </Box>
        )}
      </Stack>
    </Box>
  );
}

/** The host records how the Society ended the mystery; the engine writes it back to the world. */
function CloseModal({ opened, onClose, stored, onClosed }: { opened: boolean; onClose: () => void; stored: StoredCase; onClosed: (next: StoredCase) => void }) {
  const call = useApi();
  const p = stored.packet;
  const [outcome, setOutcome] = useState<"solved" | "failed">("solved");
  const [leverId, setLeverId] = useState<string | null>(p.resolutionLevers[0]?.id ?? null);
  const [notes, setNotes] = useState("");
  const [deedsDone, setDeedsDone] = useState<string[]>([]);
  const [error, setError] = useState<string>();
  const [saving, setSaving] = useState(false);

  const save = async () => {
    setSaving(true);
    setError(undefined);
    const request: CloseMysteryRequest = { outcome, leverId: outcome === "solved" ? leverId : null, notes, deedsDone };
    const result = await call<StoredCase>(Http.POST, MysteryApi.close(p.mysteryId), request);
    setSaving(false);
    if (!result.success || !result.result) {
      setError(result.errorMessage);
      return;
    }
    onClosed(result.result);
    onClose();
  };

  return (
    <Modal opened={opened} onClose={onClose} size="lg" title={<Text className="nx-display" fz="xl">How did it end?</Text>}>
      <Stack>
        <Text fz="sm">
          The engine writes the ending back to the Society's world: attitudes, who died or left, what changed, and the
          people of this case join the persistent cast to be met again. This cannot be undone.
        </Text>
        <SegmentedControl
          value={outcome}
          onChange={(v) => setOutcome(v as "solved" | "failed")}
          data={[{ value: "solved", label: "The Society ended it" }, { value: "failed", label: "It was left unresolved" }]}
        />
        {outcome === "solved" && (
          <Select
            label="How they ended it"
            value={leverId}
            onChange={setLeverId}
            data={p.resolutionLevers.map((l) => ({ value: l.id, label: `${l.solveDepth.charAt(0).toUpperCase()}${l.solveDepth.slice(1)}: ${l.description}` }))}
            allowDeselect={false}
          />
        )}
        <MultiSelect
          label="Deeds the Society carried out"
          value={deedsDone}
          onChange={setDeedsDone}
          data={p.resolutionLevers.flatMap((l) => (l.requiredDeeds ?? []).map((d) => ({ value: d.id, label: `${d.id}: ${d.description}` })))}
          clearable
        />
        <Textarea label="Notes for the world's memory (optional)" autosize minRows={2} value={notes} onChange={(e) => setNotes(e.currentTarget.value)} />
        <ErrorNote message={error} />
        <Group justify="flex-end">
          <Button variant="subtle" color="gold" onClick={onClose}>Cancel</Button>
          <Button onClick={save} loading={saving}>Close the mystery</Button>
        </Group>
      </Stack>
    </Modal>
  );
}
