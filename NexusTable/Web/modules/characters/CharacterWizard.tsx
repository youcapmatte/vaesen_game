"use client";

import {
  ActionIcon, Badge, Box, Button, Container, Group, NumberInput, SegmentedControl, Select, SimpleGrid, Stack, Stepper,
  Text, TextInput, Textarea, Title, Tooltip, UnstyledButton,
} from "@mantine/core";
import { IconArrowLeft, IconArrowRight, IconDice5, IconMinus, IconPlus, IconTrash, IconWand } from "@tabler/icons-react";
import { useRouter } from "next/navigation";
import { useEffect, useMemo, useState } from "react";
import { CharacterApi, type Character } from "@/common/api/character";
import { RulesApi, type RuleArchetype, type RuleMemento, type VaesenRules } from "@/common/api/rules";
import { Http, useApi } from "@/common/client/core/Api";
import { useMe, useRules, useSociety } from "@/common/client/components/hooks";
import { Corners, Divider, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame, PageTitle } from "@/common/client/components/PageFrame";
import { CharacterSheet, Pips } from "./CharacterSheet";
import {
  attributeBudget, attributeMax, type Budget, type CharacterDraft, emptyDraft, findArchetype, pick, skillBudget, skillMax,
  stepProblems, toRequest, WIZARD_STEPS, type WizardStep, withAgeGroup, withArchetype,
} from "./creation";

const DRAFT_KEY = "nexus.characterDraft";

const STEP_LABELS: Record<WizardStep, { label: string; description: string }> = {
  archetype: { label: "Archetype", description: "Your calling" },
  age: { label: "Age & name", description: "Who you are" },
  attributes: { label: "Attributes", description: "Body and mind" },
  skills: { label: "Skills", description: "And Resources" },
  talent: { label: "Talent", description: "Your gift" },
  personality: { label: "Personality", description: "Why you hunt" },
  relationships: { label: "Relationships", description: "The others" },
  belongings: { label: "Belongings", description: "Memento & gear" },
  review: { label: "Review", description: "Sign the ledger" },
};

export function CharacterWizard() {
  return (
    <PageFrame>
      <WizardContent />
    </PageFrame>
  );
}

function loadDraft(): CharacterDraft {
  try {
    const raw = window.localStorage.getItem(DRAFT_KEY);
    return raw ? { ...emptyDraft(), ...(JSON.parse(raw) as CharacterDraft) } : emptyDraft();
  } catch {
    return emptyDraft();
  }
}

/** The rulebook's twelve creation steps, grouped into nine pages. The draft survives a reload (this browser only). */
function WizardContent() {
  const { data: rules } = useRules();
  const { data: society } = useSociety();
  const { data: me } = useMe();
  const call = useApi();
  const router = useRouter();
  const [draft, setDraft] = useState<CharacterDraft>(emptyDraft);
  const [active, setActive] = useState(0);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string>();

  const [loaded, setLoaded] = useState(false);

  useEffect(() => {
    setDraft(loadDraft());
    setLoaded(true);
  }, []);
  useEffect(() => {
    // Not before the stored draft is read, or the empty one would overwrite it.
    if (!loaded) {
      return;
    }
    try {
      window.localStorage.setItem(DRAFT_KEY, JSON.stringify(draft));
    } catch {
      // Storage unavailable: the draft lasts until the page closes.
    }
  }, [draft, loaded]);

  const takenArchetypes = useMemo(
    () => society?.members.filter((m) => m.accountId !== me?.id && m.character).map((m) => m.character!.archetype) ?? [],
    [society, me],
  );

  if (!rules || !society || !me) {
    return <FullPageLoader label="Sharpening the quill…" />;
  }

  const step = WIZARD_STEPS[active];
  const problems = stepProblems(rules, draft, step, takenArchetypes);
  const update = (change: Partial<CharacterDraft>) => setDraft((d) => ({ ...d, ...change }));

  const create = async () => {
    setSaving(true);
    const result = await call<Character>(Http.POST, CharacterApi.characters, toRequest(draft));
    setSaving(false);
    if (!result.success || !result.result) {
      setError(result.errorMessage);
      return;
    }
    try {
      window.localStorage.removeItem(DRAFT_KEY);
    } catch {
      // Nothing stored.
    }
    router.push(`/characters/${result.result.id}`);
  };

  return (
    <Container size="xl">
      <PageTitle kicker="Joining the Society" title="A new member">
        Twelve questions the Society asks of everyone it sends into the dark.
      </PageTitle>

      <Stepper
        active={active}
        onStepClick={(i) => i < active && setActive(i)}
        color="blood"
        size="xs"
        mb="xl"
        styles={{ stepLabel: { fontFamily: "var(--font-display)" }, stepDescription: { color: "var(--nx-sepia)" } }}
      >
        {WIZARD_STEPS.map((s) => (
          <Stepper.Step key={s} label={STEP_LABELS[s].label} description={STEP_LABELS[s].description} />
        ))}
      </Stepper>

      <Box className="nx-panel" p={{ base: "md", sm: "xl" }} mih={420}>
        <Corners />
        {step === "archetype" && <ArchetypeStep rules={rules} draft={draft} taken={takenArchetypes} onChoose={(a) => setDraft(withArchetype(draft, rules, a))} />}
        {step === "age" && <AgeStep rules={rules} draft={draft} update={update} setDraft={setDraft} />}
        {step === "attributes" && <AttributesStep rules={rules} draft={draft} update={update} />}
        {step === "skills" && <SkillsStep rules={rules} draft={draft} update={update} />}
        {step === "talent" && <TalentStep rules={rules} draft={draft} update={update} />}
        {step === "personality" && <PersonalityStep rules={rules} draft={draft} update={update} />}
        {step === "relationships" && (
          <RelationshipsStep
            rules={rules}
            draft={draft}
            update={update}
            others={society.members.filter((m) => m.accountId !== me.id && m.character).map((m) => m.character!)}
          />
        )}
        {step === "belongings" && <BelongingsStep rules={rules} draft={draft} update={update} />}
        {step === "review" && <ReviewStep rules={rules} draft={draft} update={update} accountId={me.id} />}
      </Box>

      <Group justify="space-between" mt="lg" align="flex-start">
        <Button variant="subtle" color="gold" leftSection={<IconArrowLeft size={16} />} disabled={active === 0} onClick={() => setActive(active - 1)}>
          Back
        </Button>
        <Stack gap={2} align="flex-end">
          {step === "review" ? (
            <Button size="md" color="blood" loading={saving} disabled={problems.length > 0} onClick={create} leftSection={<SightMark size={14} />}>
              Join the Society
            </Button>
          ) : (
            <Button rightSection={<IconArrowRight size={16} />} disabled={problems.length > 0} onClick={() => setActive(active + 1)}>
              Continue
            </Button>
          )}
          {problems.slice(0, 3).map((p) => <Text key={p} fz="sm" c="var(--nx-sepia)" className="nx-flourish">{p}</Text>)}
          <ErrorNote message={error} />
        </Stack>
      </Group>
    </Container>
  );
}

function StepIntro({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <Stack gap={4} mb="lg" align="center" ta="center">
      <Title order={2}>{title}</Title>
      <Divider width={160} />
      <Text className="nx-flourish" c="var(--nx-parchment-2)" maw={640}>{children}</Text>
    </Stack>
  );
}

function BudgetBadge({ budget, label }: { budget: Budget; label: string }) {
  const color = budget.remaining === 0 ? "moss.4" : budget.remaining < 0 ? "blood.4" : "gold";
  return (
    <Badge size="lg" color={color} variant="outline">
      {label}: {budget.remaining} of {budget.total} left
    </Badge>
  );
}

function ArchetypeStep({ rules, draft, taken, onChoose }: {
  rules: VaesenRules;
  draft: CharacterDraft;
  taken: string[];
  onChoose: (archetype: RuleArchetype) => void;
}) {
  const attr = (id: string) => rules.attributes.find((a) => a.id === id)?.name;
  const skill = (id: string) => rules.skills.find((s) => s.id === id)?.name;
  return (
    <>
      <StepIntro title="Choose your calling">
        Your archetype is the skeleton of your character: what you are good at, the talents you can start with, and how
        well you live. No two members of the Society share one.
      </StepIntro>
      <SimpleGrid cols={{ base: 1, xs: 2, md: 3, lg: 5 }} spacing="md">
        {rules.archetypes.map((a) => {
          const isTaken = taken.includes(a.id);
          const chosen = draft.archetype === a.id;
          return (
            <UnstyledButton
              key={a.id}
              disabled={isTaken}
              onClick={() => !chosen && onChoose(a)}
              className="nx-parchment"
              p="md"
              style={{
                outline: chosen ? "3px solid var(--nx-blood)" : undefined,
                outlineOffset: 2,
                opacity: isTaken ? 0.4 : 1,
                transform: chosen ? "rotate(-0.6deg) translateY(-3px)" : undefined,
                transition: "transform 140ms",
                display: "flex",
                flexDirection: "column",
                gap: 6,
              }}
            >
              <Title order={4}>{a.name}</Title>
              <Text fz="sm" c="var(--nx-ink-soft)" fs="italic">{a.summary}</Text>
              <Text fz="xs" mt="auto"><b>{attr(a.mainAttribute)}</b> · {skill(a.mainSkill)}</Text>
              <Text fz="xs" c="var(--nx-ink-soft)">Resources {a.resources.min}–{a.resources.max}</Text>
              {isTaken && <Badge color="blood" size="xs">Taken</Badge>}
            </UnstyledButton>
          );
        })}
      </SimpleGrid>
    </>
  );
}

function AgeStep({ rules, draft, update, setDraft }: {
  rules: VaesenRules;
  draft: CharacterDraft;
  update: (c: Partial<CharacterDraft>) => void;
  setDraft: (d: CharacterDraft) => void;
}) {
  const archetype = findArchetype(rules, draft);
  const group = rules.ageGroups.find((g) => g.id === draft.ageGroup);
  return (
    <>
      <StepIntro title="Age and name">
        The young are quick and strong; the old have learned more. Your age decides how many points you have for
        attributes and skills.
      </StepIntro>
      <Stack maw={620} mx="auto" gap="lg">
        <SegmentedControl
          fullWidth
          color="blood"
          value={draft.ageGroup}
          onChange={(v) => setDraft(withAgeGroup(draft, rules, v))}
          data={rules.ageGroups.map((g) => ({
            value: g.id,
            label: `${g.name} · ${g.minAge}${g.maxAge === null ? "+" : `–${g.maxAge}`}`,
          }))}
        />
        {group && (
          <Text ta="center" fz="sm" c="var(--nx-parchment-2)">
            {group.attributePoints} attribute points and {group.skillPoints} skill points.
          </Text>
        )}
        <NumberInput
          label="Age"
          value={draft.age || ""}
          min={group?.minAge}
          max={group?.maxAge ?? 99}
          disabled={!group}
          onChange={(v) => update({ age: Number(v) || 0 })}
          maw={160}
        />
        <Group grow align="flex-end">
          <TextInput label="First name" value={draft.firstName} onChange={(e) => update({ firstName: e.currentTarget.value })} />
          <TextInput label="Last name" value={draft.lastName} onChange={(e) => update({ lastName: e.currentTarget.value })} />
          <Tooltip label={`A name a ${archetype?.name ?? "member"} might have`}>
            <ActionIcon
              size="lg"
              variant="outline"
              color="gold"
              style={{ flexGrow: 0 }}
              onClick={() => update({ firstName: pick(archetype?.names.first ?? []) ?? "", lastName: pick(archetype?.names.last ?? []) ?? "" })}
              aria-label="Suggest a name"
            >
              <IconWand size={18} />
            </ActionIcon>
          </Tooltip>
        </Group>
      </Stack>
    </>
  );
}

function PointStepper({ value, min, max, canRaise, onChange }: {
  value: number;
  min: number;
  max: number;
  canRaise: boolean;
  onChange: (v: number) => void;
}) {
  return (
    <Group gap={6} wrap="nowrap">
      <ActionIcon className="nx-pip-button" size="sm" variant="subtle" color="umber.9" disabled={value <= min} onClick={() => onChange(value - 1)} aria-label="Lower">
        <IconMinus size={14} />
      </ActionIcon>
      <Pips value={value} max={max} />
      <ActionIcon className="nx-pip-button" size="sm" variant="subtle" color="umber.9" disabled={value >= max || !canRaise} onClick={() => onChange(value + 1)} aria-label="Raise">
        <IconPlus size={14} />
      </ActionIcon>
    </Group>
  );
}

function AttributesStep({ rules, draft, update }: { rules: VaesenRules; draft: CharacterDraft; update: (c: Partial<CharacterDraft>) => void }) {
  const archetype = findArchetype(rules, draft);
  const budget = attributeBudget(rules, draft);
  return (
    <>
      <StepIntro title="Attributes">
        Each attribute is {rules.creationLimits.attributeMin} to {rules.creationLimits.attributeMax}; your archetype&apos;s main
        attribute can reach {rules.creationLimits.mainAttributeMax}. They are the dice you roll before any skill is added.
      </StepIntro>
      <Group justify="center" mb="md"><BudgetBadge budget={budget} label="Attribute points" /></Group>
      <SimpleGrid cols={{ base: 1, sm: 2 }} spacing="md" maw={860} mx="auto">
        {rules.attributes.map((a) => {
          const value = draft.attributes[a.id] ?? rules.creationLimits.attributeMin;
          const isMain = archetype?.mainAttribute === a.id;
          return (
            <Box key={a.id} className="nx-parchment" p="md">
              <Group justify="space-between" wrap="nowrap">
                <Box>
                  <Group gap={6}>
                    <Title order={4}>{a.name}</Title>
                    {isMain && <Badge size="xs" color="blood">Main</Badge>}
                  </Group>
                  <Text fz="sm" c="var(--nx-ink-soft)">{a.description}</Text>
                </Box>
                <Stack gap={2} align="center">
                  <Text className="nx-display" fz={28} lh={1}>{value}</Text>
                  <PointStepper
                    value={value}
                    min={rules.creationLimits.attributeMin}
                    max={attributeMax(rules, archetype, a.id)}
                    canRaise={budget.remaining > 0}
                    onChange={(v) => update({ attributes: { ...draft.attributes, [a.id]: v } })}
                  />
                </Stack>
              </Group>
            </Box>
          );
        })}
      </SimpleGrid>
    </>
  );
}

function SkillsStep({ rules, draft, update }: { rules: VaesenRules; draft: CharacterDraft; update: (c: Partial<CharacterDraft>) => void }) {
  const archetype = findArchetype(rules, draft);
  const budget = skillBudget(rules, draft);
  const resourceName = rules.resourceLevels.find((r) => r.value === draft.resources)?.name;
  return (
    <>
      <StepIntro title="Skills and Resources">
        No skill above {rules.creationLimits.skillMax} to begin with, except your main skill, which can have
        {" "}{rules.creationLimits.mainSkillMax}. A skill point can also raise your Resources one step, up to your archetype&apos;s limit.
      </StepIntro>
      <Group justify="center" mb="md"><BudgetBadge budget={budget} label="Skill points" /></Group>
      <SimpleGrid cols={{ base: 1, sm: 2, lg: 4 }} spacing="md">
        {rules.attributes.map((a) => (
          <Box key={a.id} className="nx-parchment" p="md">
            <Text className="nx-kicker" c="var(--nx-blood)" mb={6}>{a.name} {draft.attributes[a.id]}</Text>
            <Stack gap={8}>
              {rules.skills.filter((s) => s.attribute === a.id).map((s) => (
                <Group key={s.id} justify="space-between" wrap="nowrap" gap={4}>
                  <Tooltip label={s.description} multiline w={260}>
                    <Text fz="sm" fw={archetype?.mainSkill === s.id ? 700 : 400}>{s.name}</Text>
                  </Tooltip>
                  <PointStepper
                    value={draft.skills[s.id] ?? 0}
                    min={0}
                    max={skillMax(rules, archetype, s.id)}
                    canRaise={budget.remaining > 0}
                    onChange={(v) => update({ skills: { ...draft.skills, [s.id]: v } })}
                  />
                </Group>
              ))}
            </Stack>
          </Box>
        ))}
      </SimpleGrid>
      {archetype && (
        <Box className="nx-parchment" p="md" mt="md" maw={520} mx="auto">
          <Group justify="space-between" wrap="nowrap">
            <Box>
              <Title order={4}>Resources {draft.resources}</Title>
              <Text fz="sm" c="var(--nx-ink-soft)">{resourceName} · a {archetype.name} has {archetype.resources.min}–{archetype.resources.max}</Text>
            </Box>
            <PointStepper
              value={draft.resources}
              min={archetype.resources.min}
              max={archetype.resources.max}
              canRaise={budget.remaining > 0}
              onChange={(v) => update({ resources: v })}
            />
          </Group>
        </Box>
      )}
    </>
  );
}

function TalentStep({ rules, draft, update }: { rules: VaesenRules; draft: CharacterDraft; update: (c: Partial<CharacterDraft>) => void }) {
  const archetype = findArchetype(rules, draft);
  return (
    <>
      <StepIntro title="A starting talent">
        Choose one of your archetype&apos;s three talents. With experience you can later learn any talent at all.
      </StepIntro>
      <SimpleGrid cols={{ base: 1, md: 3 }} spacing="md" maw={1000} mx="auto">
        {archetype?.talents.map((id) => {
          const talent = rules.talents.find((t) => t.id === id);
          const chosen = draft.talent === id;
          return (
            <UnstyledButton
              key={id}
              onClick={() => update({ talent: id })}
              className="nx-parchment"
              p="lg"
              style={{ outline: chosen ? "3px solid var(--nx-blood)" : undefined, outlineOffset: 2, transform: chosen ? "rotate(-0.5deg)" : undefined }}
            >
              <Group gap={8} mb={6}><SightMark size={12} style={{ color: "var(--nx-blood)" }} /><Title order={4}>{talent?.name ?? id}</Title></Group>
              <Text fz="md" c="var(--nx-ink-soft)">{talent?.effect}</Text>
            </UnstyledButton>
          );
        })}
      </SimpleGrid>
    </>
  );
}

/** A suggestion from the archetype, written out in full; choosing it fills the field. */
function Suggestion({ text, chosen, onChoose }: { text: string; chosen: boolean; onChoose: () => void }) {
  return (
    <UnstyledButton
      onClick={onChoose}
      px="sm"
      py={4}
      style={{
        border: chosen ? "1px solid var(--nx-blood)" : "1px dashed rgba(43,33,24,0.4)",
        background: chosen ? "rgba(142,42,34,0.12)" : "transparent",
        color: "var(--nx-ink)",
        fontStyle: "italic",
        fontSize: "0.92rem",
        lineHeight: 1.3,
      }}
    >
      {text}
    </UnstyledButton>
  );
}

function SuggestedText({ label, hint, value, suggestions, onChange }: {
  label: string;
  hint: string;
  value: string;
  suggestions: string[];
  onChange: (v: string) => void;
}) {
  return (
    <Box className="nx-parchment" p="md">
      <Title order={4}>{label}</Title>
      <Text fz="sm" c="var(--nx-ink-soft)" mb="xs">{hint}</Text>
      <Stack gap={4} mb="xs">
        {suggestions.map((s) => <Suggestion key={s} text={s} chosen={value === s} onChoose={() => onChange(s)} />)}
      </Stack>
      <Textarea
        value={value}
        onChange={(e) => onChange(e.currentTarget.value)}
        autosize
        minRows={2}
        placeholder="Or in your own words…"
        styles={{ input: { background: "rgba(255,250,235,0.5)", color: "var(--nx-ink)", borderColor: "rgba(43,33,24,0.3)" } }}
      />
    </Box>
  );
}

function PersonalityStep({ rules, draft, update }: { rules: VaesenRules; draft: CharacterDraft; update: (c: Partial<CharacterDraft>) => void }) {
  const archetype = findArchetype(rules, draft);
  return (
    <>
      <StepIntro title="Why you hunt">
        Pick from the suggestions or write your own. The Gamemaster will make sure your dark secret finds you.
      </StepIntro>
      <SimpleGrid cols={{ base: 1, md: 3 }} spacing="md">
        <SuggestedText label="Motivation" hint="Why you risk your life to track down vaesen." value={draft.motivation} suggestions={archetype?.motivations ?? []} onChange={(v) => update({ motivation: v })} />
        <SuggestedText label="Trauma" hint="The event that gave you the Sight." value={draft.trauma} suggestions={archetype?.traumas ?? []} onChange={(v) => update({ trauma: v })} />
        <SuggestedText label="Dark secret" hint="Something you are ashamed of. Only you and the host see it." value={draft.darkSecret} suggestions={archetype?.darkSecrets ?? []} onChange={(v) => update({ darkSecret: v })} />
      </SimpleGrid>
    </>
  );
}

function RelationshipsStep({ rules, draft, update, others }: {
  rules: VaesenRules;
  draft: CharacterDraft;
  update: (c: Partial<CharacterDraft>) => void;
  others: Character[];
}) {
  const archetype = findArchetype(rules, draft);

  // Start with a line for every member who already has a character.
  useEffect(() => {
    const missing = others.filter((c) => !draft.relationships.some((r) => r.characterId === c.id));
    if (missing.length > 0) {
      update({ relationships: [...draft.relationships, ...missing.map((c) => ({ characterId: c.id, name: c.name, text: "" }))] });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [others]);

  const setAt = (i: number, change: Partial<CharacterDraft["relationships"][number]>) =>
    update({ relationships: draft.relationships.map((r, j) => (j === i ? { ...r, ...change } : r)) });

  return (
    <>
      <StepIntro title="The others">
        You all know each other already. Say how you feel about each of the other members; they must be people you
        can travel with. Members who join later can be added from your sheet.
      </StepIntro>
      <Stack maw={820} mx="auto" gap="md">
        {draft.relationships.length === 0 && (
          <Text ta="center" c="var(--nx-sepia)" className="nx-flourish">No one else has made a character yet.</Text>
        )}
        {draft.relationships.map((r, i) => (
          <Box key={i} className="nx-parchment" p="md">
            <Group align="flex-end" wrap="nowrap">
              <TextInput
                label="Who"
                value={r.name}
                readOnly={Boolean(r.characterId)}
                onChange={(e) => setAt(i, { name: e.currentTarget.value })}
                w={220}
                styles={{ label: { color: "var(--nx-ink)" } }}
              />
              <TextInput
                label="How you feel about them"
                value={r.text}
                onChange={(e) => setAt(i, { text: e.currentTarget.value })}
                style={{ flex: 1 }}
                styles={{ label: { color: "var(--nx-ink)" } }}
              />
              <ActionIcon variant="subtle" color="blood" onClick={() => update({ relationships: draft.relationships.filter((_, j) => j !== i) })} aria-label="Remove">
                <IconTrash size={16} />
              </ActionIcon>
            </Group>
            <Group gap={6} mt="xs" align="stretch">
              {archetype?.relationships.map((s) => <Suggestion key={s} text={s} chosen={r.text === s} onChoose={() => setAt(i, { text: s })} />)}
            </Group>
          </Box>
        ))}
        <Button variant="subtle" color="gold" leftSection={<IconPlus size={16} />} onClick={() => update({ relationships: [...draft.relationships, { name: "", text: "" }] })}>
          Add someone
        </Button>
      </Stack>
    </>
  );
}

function BelongingsStep({ rules, draft, update }: { rules: VaesenRules; draft: CharacterDraft; update: (c: Partial<CharacterDraft>) => void }) {
  const archetype = findArchetype(rules, draft);
  const call = useApi();
  const [rolled, setRolled] = useState<number>();

  const roll = async () => {
    const result = await call<RuleMemento>(Http.POST, RulesApi.memento);
    if (result.success && result.result) {
      setRolled(result.result.roll);
      update({ memento: result.result.item });
    }
  };

  return (
    <>
      <StepIntro title="What you carry">
        A memento helps you portray your character, and handling it can heal a Condition. Then take your
        archetype&apos;s equipment.
      </StepIntro>
      <SimpleGrid cols={{ base: 1, md: 2 }} spacing="lg">
        <Box className="nx-parchment" p="lg">
          <Title order={4}>Memento</Title>
          <Text fz="sm" c="var(--nx-ink-soft)" mb="sm">Roll two dice, tens and ones, or choose.</Text>
          <Group mb="sm">
            <Button color="blood" leftSection={<IconDice5 size={18} />} onClick={roll}>Roll d66</Button>
            {rolled && <Badge size="lg" color="umber.8">{rolled}</Badge>}
          </Group>
          <Select
            placeholder="Choose from the table"
            searchable
            value={rules.mementos.find((m) => m.item === draft.memento)?.item ?? null}
            data={rules.mementos.map((m) => ({ value: m.item, label: `${m.roll} · ${m.item}` }))}
            onChange={(v) => v && update({ memento: v })}
            mb="xs"
          />
          <TextInput
            value={draft.memento}
            onChange={(e) => update({ memento: e.currentTarget.value })}
            placeholder="Or something of your own"
            styles={{ input: { background: "rgba(255,250,235,0.5)", color: "var(--nx-ink)", borderColor: "rgba(43,33,24,0.3)" } }}
          />
        </Box>
        <Box className="nx-parchment" p="lg">
          <Title order={4}>Equipment</Title>
          <Text fz="sm" c="var(--nx-ink-soft)" mb="sm">One of each.</Text>
          <Stack gap="sm">
            {archetype?.equipment.map((options, i) =>
              options.length === 1 ? (
                <Text key={i} className="nx-display">{options[0]}</Text>
              ) : (
                <SegmentedControl
                  key={i}
                  color="blood"
                  value={draft.equipment[i] ?? options[0]}
                  data={options}
                  onChange={(v) => update({ equipment: draft.equipment.map((e, j) => (j === i ? v : e)) })}
                />
              ))}
          </Stack>
        </Box>
      </SimpleGrid>
    </>
  );
}

function ReviewStep({ rules, draft, update, accountId }: {
  rules: VaesenRules;
  draft: CharacterDraft;
  update: (c: Partial<CharacterDraft>) => void;
  accountId: string;
}) {
  const request = toRequest(draft);
  const preview: Character = {
    ...request,
    id: "preview",
    accountId,
    societyId: "",
    skills: draft.skills,
    talents: draft.talent ? [draft.talent] : [],
    conditions: [],
    physicallyBroken: false,
    mentallyBroken: false,
    experience: 0,
    insightsAndDefects: [],
    notes: "",
    createdAt: "",
    updatedAt: "",
    version: 0,
  };
  return (
    <>
      <StepIntro title="Sign the ledger">
        Describe yourself as the others will see you: how you look, how you are addressed, what people whisper.
      </StepIntro>
      <Stack gap="lg">
        <Textarea
          label="Description"
          placeholder="Tall and quiet, smells of pipe smoke and old paper…"
          value={draft.description}
          onChange={(e) => update({ description: e.currentTarget.value })}
          autosize
          minRows={2}
          maw={820}
          mx="auto"
          w="100%"
        />
        <CharacterSheet character={preview} rules={rules} canEdit={false} canSeeSecret />
      </Stack>
    </>
  );
}
