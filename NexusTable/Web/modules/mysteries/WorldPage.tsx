"use client";

import { Accordion, Badge, Box, Button, Container, Group, Loader, SegmentedControl, SimpleGrid, Stack, Text, Title } from "@mantine/core";
import { IconFileCode, IconUsers } from "@tabler/icons-react";
import { useEffect, useRef, useState } from "react";
import { attitudeName, MysteryApi, TierNames, WorldApi, type CharacterInstance, type MysteryJob, type SocietyWorld, type WorldTier } from "@/common/api/mystery";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { useMe } from "@/common/client/components/hooks";
import { Corners, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame, PageTitle } from "@/common/client/components/PageFrame";
import { HostOnly } from "./CaseFilesPage";

/** The Society's persistent world: everyone it will meet again, what they think of it and what they remember. Host only. */
export function WorldPage() {
  return (
    <PageFrame>
      <WorldContent />
    </PageFrame>
  );
}

function WorldContent() {
  const { data: me } = useMe();
  if (!me) {
    return <FullPageLoader label="Gathering the Society's acquaintances…" />;
  }
  return me.role === "Host" ? <HostWorld /> : <HostOnly />;
}

const TierOrder: WorldTier[] = ["canonical", "core", "background", "locationResident"];

function HostWorld() {
  const { data: world, error, mutate } = useApiData<SocietyWorld>(WorldApi.world, { shouldRetryOnError: false });
  const [tier, setTier] = useState<WorldTier | "all">("all");
  const notSeeded = error?.status === 404;

  return (
    <Container size="lg">
      <PageTitle kicker="Host only" title="The Society's World">
        The people this Society meets again and again: what they think of it, what they remember, and what the Society
        has changed. Their secrets are written here too.
      </PageTitle>

      {notSeeded && <SeedPanel onSeeded={() => void mutate()} />}
      {error && !notSeeded && <ErrorNote message={error.message} />}
      {!world && !error && <Group justify="center"><Loader color="gold" type="dots" /></Group>}

      {world && (
        <Stack gap="lg">
          <Group justify="space-between" wrap="wrap">
            <Text fz="sm" c="var(--nx-parchment-2)" className="nx-flourish">
              Seeded {new Date(world.seededAt).toLocaleDateString(undefined, { dateStyle: "medium" })} · {world.characters.length} people
              · {world.characters.filter((c) => c.status === "dead").length} dead · {world.closedMysteryIds.length} mysteries closed
            </Text>
            <Button size="xs" variant="outline" color="gold" leftSection={<IconFileCode size={14} />} onClick={() => download(world)}>
              Download the world (JSON)
            </Button>
          </Group>
          <SegmentedControl
            value={tier}
            onChange={(v) => setTier(v as WorldTier | "all")}
            data={[{ value: "all", label: "Everyone" }, ...TierOrder.map((t) => ({ value: t, label: `${TierNames[t]} · ${world.characters.filter((c) => c.tier === t).length}` }))]}
            fullWidth
          />
          {TierOrder.filter((t) => tier === "all" || tier === t).map((t) => {
            const people = world.characters.filter((c) => c.tier === t).sort((a, b) => b.importance - a.importance || a.name.localeCompare(b.name));
            return people.length === 0 ? null : (
              <Box key={t}>
                <Text className="nx-kicker" mb="sm">{TierNames[t]}</Text>
                <SimpleGrid cols={{ base: 1, sm: 2 }} spacing="md">
                  {people.map((c) => <PersonCard key={c.id} person={c} world={world} />)}
                </SimpleGrid>
              </Box>
            );
          })}
          {world.overlay.length > 0 && (
            <Box className="nx-parchment" p="lg">
              <Title order={3} mb="xs">What the Society has changed</Title>
              {world.overlay.map((o, i) => (
                <Text key={i} fz="sm">· {o.levelId ? `${o.levelId}: ` : ""}{o.text}</Text>
              ))}
            </Box>
          )}
        </Stack>
      )}
    </Container>
  );
}

function PersonCard({ person: c, world }: { person: CharacterInstance; world: SocietyWorld }) {
  const name = (id: string) => world.characters.find((x) => x.id === id)?.name ?? id;
  const gone = c.status === "dead" || c.status === "departed";
  return (
    <Box className="nx-parchment" p="md" style={{ opacity: gone ? 0.7 : 1 }}>
      <Group justify="space-between" align="flex-start" wrap="nowrap">
        <Box>
          <Title order={4} td={c.status === "dead" ? "line-through" : undefined}>{c.name}</Title>
          <Text fz="xs" c="var(--nx-ink-soft)">
            {[c.identity.age ? `${c.identity.age}` : "", c.identity.occupation, c.home].filter(Boolean).join(" · ")}
          </Text>
        </Box>
        <Stack gap={4} align="flex-end">
          <Badge size="sm" color={c.attitude > 0 ? "moss" : c.attitude < 0 ? "blood" : "umber"}>
            {attitudeName(c.attitude)} {c.attitude > 0 ? `+${c.attitude}` : c.attitude}
          </Badge>
          {c.status !== "alive" && <Badge size="xs" variant="outline" color="blood">{c.status}</Badge>}
        </Stack>
      </Group>
      {c.attitudeReason && <Text fz="xs" fs="italic" mt={4}>{c.attitudeReason}</Text>}
      <Accordion variant="default" mt="xs" chevronPosition="left" styles={{ control: { padding: 0 }, content: { paddingLeft: 0, paddingRight: 0 } }}>
        <Accordion.Item value="more" style={{ borderBottom: "none" }}>
          <Accordion.Control>
            <Text fz="xs" c="var(--nx-ink-soft)">
              {c.memories.length} memor{c.memories.length === 1 ? "y" : "ies"} · {c.mysteryIds.length} myster{c.mysteryIds.length === 1 ? "y" : "ies"} · importance {c.importance}
            </Text>
          </Accordion.Control>
          <Accordion.Panel>
            <Stack gap={6}>
              {c.drives.length > 0 && <Line label="Wants">{c.drives.map((d) => d.text).join("; ")}</Line>}
              {c.voice.register && <Line label="Voice">{c.voice.register}</Line>}
              {c.background && <Line label="Background">{c.background}</Line>}
              {c.latentHooks.length > 0 && <Line label="Hooks the engine can use">{c.latentHooks.map((h) => `${h.kind}: ${h.text}`).join(" · ")}</Line>}
              {c.relationships.length > 0 && (
                <Line label="Ties">{c.relationships.map((r) => `${name(r.characterId)} (${r.nature}, ${r.sentiment > 0 ? "+" : ""}${r.sentiment})`).join("; ")}</Line>
              )}
              {c.memories.map((m, i) => <Line key={i} label={`Remembers · ${m.when}`}>{m.text}</Line>)}
              {c.knowledge.length > 0 && <Line label="Knows from before">{c.knowledge.map((k) => k.statement).join(" · ")}</Line>}
              {c.beliefs.length > 0 && <Line label="Thinks">{c.beliefs.join(" · ")}</Line>}
            </Stack>
          </Accordion.Panel>
        </Accordion.Item>
      </Accordion>
    </Box>
  );
}

function Line({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <Box>
      <Text className="nx-kicker" c="var(--nx-blood)" fz={10}>{label}</Text>
      <Text fz="sm">{children}</Text>
    </Box>
  );
}

/** Before the first mystery: seed the world now, or let the first mystery do it. */
function SeedPanel({ onSeeded }: { onSeeded: () => void }) {
  const call = useApi();
  const [starting, setStarting] = useState(false);
  const [startError, setStartError] = useState<string>();
  const { data: job, mutate } = useApiData<MysteryJob | null>(MysteryApi.currentJob, {
    refreshInterval: (current) => (current?.status === "running" ? 2000 : 0),
  });
  const running = job?.status === "running";

  const lastStatus = useRef(job?.status);
  useEffect(() => {
    if (lastStatus.current === "running" && job?.status === "succeeded") {
      onSeeded();
    }
    lastStatus.current = job?.status;
  }, [job, onSeeded]);

  const seed = async () => {
    setStarting(true);
    setStartError(undefined);
    const result = await call<MysteryJob>(Http.POST, WorldApi.seed);
    setStarting(false);
    if (!result.success) {
      setStartError(result.errorMessage);
      return;
    }
    await mutate(result.result ?? null, { revalidate: false });
  };

  return (
    <Box className="nx-panel" p="xl" maw={680} mx="auto" ta="center">
      <Corners />
      <Stack align="center" gap="sm">
        <SightMark size={22} />
        <Title order={3}>The world has not been seeded yet</Title>
        <Text className="nx-flourish" c="var(--nx-parchment-2)" maw={540}>
          Before its first mystery the Society gets its own copy of everyone every Society knows, and its own circle:
          a dozen people who matter from the start and more in the background. The first mystery does this by itself,
          or you can do it now.
        </Text>
        {running ? (
          <Group gap="xs"><Loader size="sm" color="gold" /><Text fz="sm">{job.progress.at(-1)?.replace(/^[a-z]+:\s*/, "") ?? "Working…"}</Text></Group>
        ) : (
          <Button color="blood" leftSection={<IconUsers size={16} />} onClick={seed} loading={starting}>Seed the world now</Button>
        )}
        <ErrorNote message={startError ?? (job?.status === "failed" ? job.error ?? undefined : undefined)} />
      </Stack>
    </Box>
  );
}

function download(world: SocietyWorld) {
  const url = URL.createObjectURL(new Blob([JSON.stringify(world, null, 2)], { type: "application/json" }));
  const link = document.createElement("a");
  link.href = url;
  link.download = `world-${world.societyId}.json`;
  link.click();
  URL.revokeObjectURL(url);
}
