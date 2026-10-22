"use client";

import { Badge, Box, Checkbox, Group, SimpleGrid, Stack, Text, Textarea, Title, Tooltip } from "@mantine/core";
import { useDebouncedCallback } from "@mantine/hooks";
import { useState } from "react";
import { CharacterApi, type Character } from "@/common/api/character";
import type { VaesenRules } from "@/common/api/rules";
import { Http, useApi } from "@/common/client/core/Api";
import { ruleNames } from "@/common/client/components/hooks";
import { Divider, SightMark } from "@/common/client/components/Ornament";

/** Filled and empty dots for a value, like the circles on the printed sheet. */
export function Pips({ value, max = 5, size = 11, color = "var(--nx-ink)" }: { value: number; max?: number; size?: number; color?: string }) {
  return (
    <Group gap={3} wrap="nowrap" aria-label={`${value} of ${max}`}>
      {Array.from({ length: max }, (_, i) => (
        <Box
          key={i}
          w={size}
          h={size}
          style={{ borderRadius: "50%", border: `1px solid ${color}`, background: i < value ? color : "transparent", opacity: i < value ? 0.9 : 0.45 }}
        />
      ))}
    </Group>
  );
}

function SheetHeading({ children }: { children: React.ReactNode }) {
  return (
    <Text className="nx-kicker" c="var(--nx-blood)" fz={12} mb={4} style={{ borderBottom: "1px solid rgba(43,33,24,0.25)" }}>
      {children}
    </Text>
  );
}

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <Box>
      <SheetHeading>{label}</SheetHeading>
      <Text fz="md" style={{ whiteSpace: "pre-wrap" }}>{children || <Text span c="var(--nx-sepia)" fs="italic">—</Text>}</Text>
    </Box>
  );
}

/**
 * The character sheet on parchment, laid out like the rulebook's: attributes with their skills, Conditions,
 * talents, and the personal parts. The owner and the host can mark Conditions and write notes and the Advantage.
 * The dark secret is shown only to its owner and the host.
 */
export function CharacterSheet({ character, rules, canEdit, canSeeSecret, onChange, compact }: {
  character: Character;
  rules: VaesenRules;
  canEdit: boolean;
  canSeeSecret: boolean;
  onChange?: (character: Character) => void;
  compact?: boolean;
}) {
  const call = useApi();
  const names = ruleNames(rules);
  const archetype = rules.archetypes.find((a) => a.id === character.archetype);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string>();

  const setCondition = async (condition: string, active: boolean) => {
    setSaving(true);
    const result = await call<Character>(Http.PUT, CharacterApi.conditions(character.id), { condition, active });
    setSaving(false);
    setError(result.success ? undefined : result.errorMessage);
    if (result.success && result.result) {
      onChange?.(result.result);
    }
  };

  const saveText = useDebouncedCallback(async (field: "notes" | "advantage", value: string) => {
    const result = await call<Character>(Http.PATCH, CharacterApi.character(character.id), { [field]: value });
    setError(result.success ? undefined : result.errorMessage);
    if (result.success && result.result) {
      onChange?.(result.result);
    }
  }, 800);

  const conditionGroup = (kind: "physical" | "mental") => {
    const list = rules.conditions[kind];
    const broken = kind === "physical" ? character.physicallyBroken : character.mentallyBroken;
    return (
      <Box>
        <Group justify="space-between" mb={4}>
          <Text fz="xs" className="nx-display" c="var(--nx-ink-soft)">
            {kind === "physical" ? "Physical · Physique & Precision" : "Mental · Logic & Empathy"}
          </Text>
          {broken && <Badge color="blood" size="sm">Broken</Badge>}
        </Group>
        <Stack gap={4}>
          {list.map((c) => (
            <Checkbox
              key={c.id}
              label={c.name}
              color="blood"
              disabled={!canEdit || saving}
              checked={character.conditions.includes(c.id)}
              onChange={(e) => setCondition(c.id, e.currentTarget.checked)}
              styles={{ label: { color: "var(--nx-ink)", fontSize: "1rem" } }}
            />
          ))}
        </Stack>
      </Box>
    );
  };

  return (
    <Box className="nx-parchment" p={compact ? "md" : { base: "lg", sm: 36 }}>
      <Stack gap="lg">
        <Group justify="space-between" align="flex-end" wrap="wrap">
          <Box>
            <Text className="nx-kicker" c="var(--nx-sepia)">{names.archetype(character.archetype)} · {names.ageGroup(character.ageGroup)}, {character.age} years</Text>
            <Title order={compact ? 3 : 2}>{character.name}</Title>
          </Box>
          <Group gap="lg">
            <Box ta="center">
              <Text fz="xs" className="nx-display" c="var(--nx-ink-soft)">Resources</Text>
              <Tooltip label={names.resources(character.resources)}>
                <Text className="nx-display" fz={26} lh={1}>{character.resources}</Text>
              </Tooltip>
            </Box>
            <Box ta="center">
              <Text fz="xs" className="nx-display" c="var(--nx-ink-soft)">Experience</Text>
              <Text className="nx-display" fz={26} lh={1}>{character.experience}</Text>
            </Box>
          </Group>
        </Group>

        <Divider width={200} style={{ color: "var(--nx-sepia)" }} />

        <SimpleGrid cols={{ base: 1, xs: 2, md: compact ? 2 : 4 }} spacing="md">
          {rules.attributes.map((attribute) => {
            const isMain = archetype?.mainAttribute === attribute.id;
            return (
              <Box key={attribute.id} p="sm" style={{ border: "1px solid rgba(43,33,24,0.3)", background: "rgba(255,250,235,0.25)" }}>
                <Group justify="space-between" mb={6} wrap="nowrap">
                  <Text className="nx-display" fz="lg" c={isMain ? "var(--nx-blood)" : undefined}>
                    {attribute.name}
                  </Text>
                  <Text className="nx-display" fz={24} lh={1}>{character.attributes[attribute.id] ?? 0}</Text>
                </Group>
                <Stack gap={4}>
                  {rules.skills.filter((s) => s.attribute === attribute.id).map((skill) => (
                    <Group key={skill.id} justify="space-between" wrap="nowrap" gap={6}>
                      <Tooltip label={skill.description} multiline w={260}>
                        <Text fz="sm" fw={archetype?.mainSkill === skill.id ? 700 : 400}>{skill.name}</Text>
                      </Tooltip>
                      <Pips value={character.skills[skill.id] ?? 0} size={9} />
                    </Group>
                  ))}
                </Stack>
              </Box>
            );
          })}
        </SimpleGrid>

        <SimpleGrid cols={{ base: 1, sm: 2 }} spacing="lg">
          <Box>
            <SheetHeading>Conditions</SheetHeading>
            <SimpleGrid cols={2} spacing="sm">
              {conditionGroup("physical")}
              {conditionGroup("mental")}
            </SimpleGrid>
            {error && <Text c="blood.7" fz="sm" mt={4}>{error}</Text>}
          </Box>
          <Box>
            <SheetHeading>Talents</SheetHeading>
            <Stack gap={6}>
              {character.talents.map((id) => {
                const talent = rules.talents.find((t) => t.id === id);
                return (
                  <Box key={id}>
                    <Group gap={6}><SightMark size={10} style={{ color: "var(--nx-blood)" }} /><Text className="nx-display">{talent?.name ?? id}</Text></Group>
                    <Text fz="sm" c="var(--nx-ink-soft)" pl={16}>{talent?.effect}</Text>
                  </Box>
                );
              })}
            </Stack>
          </Box>
        </SimpleGrid>

        <SimpleGrid cols={{ base: 1, sm: compact ? 1 : 2 }} spacing="lg">
          <Field label="Motivation">{character.motivation}</Field>
          <Field label="Trauma">{character.trauma}</Field>
          {canSeeSecret && <Field label="Dark secret (only you and the host see this)">{character.darkSecret}</Field>}
          <Field label="Memento">{character.memento}</Field>
          <Field label="Equipment">{character.equipment.join(", ")}</Field>
          <Box>
            <SheetHeading>Relationships</SheetHeading>
            {character.relationships.length === 0 ? (
              <Text c="var(--nx-sepia)" fs="italic">None written yet.</Text>
            ) : (
              character.relationships.map((r, i) => (
                <Text key={i} fz="md"><Text span className="nx-display">{r.name}:</Text> {r.text}</Text>
              ))
            )}
          </Box>
          {character.description && <Field label="Description">{character.description}</Field>}
          {character.insightsAndDefects.length > 0 && <Field label="Insights & defects">{character.insightsAndDefects.join("\n")}</Field>}
        </SimpleGrid>

        {canEdit && (
          <SimpleGrid cols={{ base: 1, sm: 2 }} spacing="lg">
            <Textarea
              label="Advantage this mystery (+2 dice once per session)"
              placeholder="e.g. Miss Silfverstjerna trusts me"
              defaultValue={character.advantage ?? ""}
              onChange={(e) => saveText("advantage", e.currentTarget.value)}
              autosize
              minRows={2}
              styles={{ label: { color: "var(--nx-ink)" }, input: { background: "rgba(255,250,235,0.5)", color: "var(--nx-ink)", borderColor: "rgba(43,33,24,0.3)" } }}
            />
            <Textarea
              label="Notes"
              placeholder="Names, places, what the priest wasn't saying…"
              defaultValue={character.notes}
              onChange={(e) => saveText("notes", e.currentTarget.value)}
              autosize
              minRows={2}
              styles={{ label: { color: "var(--nx-ink)" }, input: { background: "rgba(255,250,235,0.5)", color: "var(--nx-ink)", borderColor: "rgba(43,33,24,0.3)" } }}
            />
          </SimpleGrid>
        )}
      </Stack>
    </Box>
  );
}
