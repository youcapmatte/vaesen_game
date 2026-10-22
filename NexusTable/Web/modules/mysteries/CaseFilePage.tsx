"use client";

import { Accordion, Badge, Box, Button, Container, Group, Progress, SimpleGrid, Stack, Table, Tabs, Text, Title } from "@mantine/core";
import { IconAlertTriangle, IconArrowLeft, IconDownload, IconEye } from "@tabler/icons-react";
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
  type CastMember,
  type CoherenceIssue,
  type StoredCase,
} from "@/common/api/mystery";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { useMe } from "@/common/client/components/hooks";
import { Corners } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame } from "@/common/client/components/PageFrame";
import { HostOnly } from "./CaseFilesPage";

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
  const { data: stored, error } = useApiData<StoredCase>(isHost ? MysteryApi.case(mysteryId) : null);
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
      {stored && (revealed ? <CaseFile stored={stored} /> : <SpoilerGate packet={stored.packet} onReveal={() => setRevealed(true)} />)}
    </Container>
  );
}

function SpoilerGate({ packet, onReveal }: { packet: CasePacket; onReveal: () => void }) {
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
      </Stack>
    </Box>
  );
}

function CaseFile({ stored }: { stored: StoredCase }) {
  const p = stored.packet;
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
        </Group>
      </Stack>

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
  const order = (c: CastMember) => (c.tier === "significant" ? 0 : 1);
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
              <Badge size="xs" color={c.tier === "significant" ? "blood" : "umber"}>{c.plotRole || c.tier}</Badge>
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
              <Group gap={6}><Badge size="xs" color={r.solveDepth === "deep" ? "moss" : "blood"}>{r.solveDepth} solve</Badge><Text fz="xs" c="var(--nx-ink-soft)">needs {r.requiresFactIds.join(", ") || "nothing"}</Text></Group>
              <Text fz="sm" mt={2}>{r.description}</Text>
              {r.consequences.world.length > 0 && <Text fz="xs" c="var(--nx-ink-soft)" fs="italic">{r.consequences.world.join(" ")}</Text>}
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

  const download = async () => {
    const result = await call<string>(Http.GET, MysteryApi.report(p.mysteryId));
    if (!result.success || !result.result) {
      setDownloadError(result.errorMessage);
      return;
    }
    const url = URL.createObjectURL(new Blob([result.result], { type: "text/markdown" }));
    const link = document.createElement("a");
    link.href = url;
    link.download = `${p.title || p.mysteryId}.md`;
    link.click();
    URL.revokeObjectURL(url);
  };

  return (
    <Stack gap="lg">
      <Sheet title="What the checks found">
        {issues.length === 0 && <Text fz="sm">Nothing: every structural check and the critic passed.</Text>}
        <Stack gap="xs">
          {issues.map((issue, i) => <IssueLine key={i} issue={issue} />)}
        </Stack>
        <Group mt="md">
          <Button variant="outline" color="umber" size="xs" leftSection={<IconDownload size={14} />} onClick={download}>Download the report (Markdown)</Button>
          <ErrorNote message={downloadError} />
        </Group>
      </Sheet>
      <Sheet title="How it was written">
        <Text fz="xs" c="var(--nx-ink-soft)" mb="xs">
          {p.models.creative}{p.models.critic ? `, critic ${p.models.critic}` : ""} · {totals.input.toLocaleString()} tokens in ({totals.cached.toLocaleString()} cached),
          {" "}{totals.output.toLocaleString()} out · {(totals.ms / 1000).toFixed(0)} s · {p.validation.runs} checks
          {stored.brief.notes.length > 0 ? ` · ${stored.brief.notes.join(" ")}` : ""}
        </Text>
        <Table fz="xs" striped withRowBorders={false}>
          <Table.Thead>
            <Table.Tr><Table.Th>Step</Table.Th><Table.Th>Try</Table.Th><Table.Th>Result</Table.Th><Table.Th ta="right">Out</Table.Th><Table.Th ta="right">Seconds</Table.Th></Table.Tr>
          </Table.Thead>
          <Table.Tbody>
            {stored.log.map((e, i) => (
              <Table.Tr key={i}>
                <Table.Td>{e.caseAttempt > 0 ? `draft ${e.caseAttempt + 1}: ` : ""}{e.step}</Table.Td>
                <Table.Td>{e.attempt + 1}</Table.Td>
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
      </Group>
      <Text fz="sm">{issue.explanation}</Text>
      {issue.suggestedFix && <Text fz="xs" fs="italic" c="var(--nx-ink-soft)">{issue.suggestedFix}</Text>}
    </Box>
  );
}
