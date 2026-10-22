import type { CharacterRelationship, CreateCharacterRequest } from "@/common/api/character";
import type { RuleArchetype, VaesenRules } from "@/common/api/rules";

/**
 * The character wizard's rules, the same as the API's (Nexus.Table/Services/CharacterRules.cs), so players see
 * their points and mistakes as they go. The API checks again on save.
 */
export interface CharacterDraft {
  archetype: string;
  ageGroup: string;
  age: number;
  firstName: string;
  lastName: string;
  attributes: Record<string, number>;
  skills: Record<string, number>;
  resources: number;
  talent: string;
  motivation: string;
  trauma: string;
  darkSecret: string;
  relationships: CharacterRelationship[];
  memento: string;
  /** One item per equipment pick of the archetype. */
  equipment: string[];
  description: string;
}

export const WIZARD_STEPS = [
  "archetype",
  "age",
  "attributes",
  "skills",
  "talent",
  "personality",
  "relationships",
  "belongings",
  "review",
] as const;

export type WizardStep = (typeof WIZARD_STEPS)[number];

export function emptyDraft(): CharacterDraft {
  return {
    archetype: "",
    ageGroup: "",
    age: 0,
    firstName: "",
    lastName: "",
    attributes: {},
    skills: {},
    resources: 0,
    talent: "",
    motivation: "",
    trauma: "",
    darkSecret: "",
    relationships: [],
    memento: "",
    equipment: [],
    description: "",
  };
}

/** Choosing an archetype starts its numbers over: every attribute at the minimum, no skills, the lowest Resources. */
export function withArchetype(draft: CharacterDraft, rules: VaesenRules, archetype: RuleArchetype): CharacterDraft {
  return {
    ...draft,
    archetype: archetype.id,
    attributes: Object.fromEntries(rules.attributes.map((a) => [a.id, rules.creationLimits.attributeMin])),
    skills: Object.fromEntries(rules.skills.map((s) => [s.id, 0])),
    resources: archetype.resources.min,
    talent: "",
    equipment: archetype.equipment.map((pick) => pick[0]),
  };
}

export function withAgeGroup(draft: CharacterDraft, rules: VaesenRules, ageGroupId: string): CharacterDraft {
  const group = rules.ageGroups.find((g) => g.id === ageGroupId);
  if (!group) {
    return draft;
  }
  const inRange = draft.age >= group.minAge && (group.maxAge === null || draft.age <= group.maxAge);
  return { ...draft, ageGroup: group.id, age: inRange ? draft.age : Math.round((group.minAge + (group.maxAge ?? group.minAge + 14)) / 2) };
}

export function findArchetype(rules: VaesenRules, draft: CharacterDraft) {
  return rules.archetypes.find((a) => a.id === draft.archetype);
}

export function attributeMax(rules: VaesenRules, archetype: RuleArchetype | undefined, attributeId: string) {
  return archetype?.mainAttribute === attributeId ? rules.creationLimits.mainAttributeMax : rules.creationLimits.attributeMax;
}

export function skillMax(rules: VaesenRules, archetype: RuleArchetype | undefined, skillId: string) {
  return archetype?.mainSkill === skillId ? rules.creationLimits.mainSkillMax : rules.creationLimits.skillMax;
}

export interface Budget {
  total: number;
  spent: number;
  remaining: number;
}

export function attributeBudget(rules: VaesenRules, draft: CharacterDraft): Budget {
  const total = rules.ageGroups.find((g) => g.id === draft.ageGroup)?.attributePoints ?? 0;
  const spent = Object.values(draft.attributes).reduce((sum, v) => sum + v, 0);
  return { total, spent, remaining: total - spent };
}

/** Skill points, including those spent raising Resources above the archetype's starting value. */
export function skillBudget(rules: VaesenRules, draft: CharacterDraft): Budget {
  const total = rules.ageGroups.find((g) => g.id === draft.ageGroup)?.skillPoints ?? 0;
  const archetype = findArchetype(rules, draft);
  const onResources = archetype ? Math.max(0, draft.resources - archetype.resources.min) : 0;
  const spent = Object.values(draft.skills).reduce((sum, v) => sum + v, 0) + onResources;
  return { total, spent, remaining: total - spent };
}

/** What still blocks a step, readable for the player; empty when the step is done. */
export function stepProblems(rules: VaesenRules, draft: CharacterDraft, step: WizardStep, takenArchetypes: string[] = []): string[] {
  const archetype = findArchetype(rules, draft);
  const problems: string[] = [];
  switch (step) {
    case "archetype":
      if (!archetype) problems.push("Choose an archetype.");
      else if (takenArchetypes.includes(archetype.id)) problems.push(`The Society already has its ${archetype.name}.`);
      break;
    case "age": {
      const group = rules.ageGroups.find((g) => g.id === draft.ageGroup);
      if (!group) problems.push("Choose an age group.");
      else if (draft.age < group.minAge || (group.maxAge !== null && draft.age > group.maxAge)) {
        problems.push(`A ${group.name.toLowerCase()} character is ${group.minAge}${group.maxAge === null ? "+" : `-${group.maxAge}`} years old.`);
      }
      if (!draft.firstName.trim() || !draft.lastName.trim()) problems.push("Give your character a first and last name.");
      break;
    }
    case "attributes": {
      const budget = attributeBudget(rules, draft);
      if (budget.remaining !== 0) problems.push(budget.remaining > 0 ? `${budget.remaining} attribute points left to spend.` : `${-budget.remaining} attribute points too many.`);
      break;
    }
    case "skills": {
      const budget = skillBudget(rules, draft);
      if (budget.remaining !== 0) problems.push(budget.remaining > 0 ? `${budget.remaining} skill points left to spend.` : `${-budget.remaining} skill points too many.`);
      break;
    }
    case "talent":
      if (!archetype?.talents.includes(draft.talent)) problems.push("Choose a starting talent.");
      break;
    case "personality":
      if (!draft.motivation.trim()) problems.push("Write your motivation.");
      if (!draft.trauma.trim()) problems.push("Write the trauma that gave you the Sight.");
      if (!draft.darkSecret.trim()) problems.push("Write your dark secret.");
      break;
    case "relationships":
      if (draft.relationships.some((r) => !r.name.trim() || !r.text.trim())) problems.push("Every relationship needs a name and a feeling.");
      break;
    case "belongings":
      if (!draft.memento.trim()) problems.push("Roll or choose a memento.");
      if (!archetype || draft.equipment.length !== archetype.equipment.length) problems.push("Choose your equipment.");
      break;
    case "review":
      for (const s of WIZARD_STEPS.filter((x) => x !== "review")) {
        problems.push(...stepProblems(rules, draft, s, takenArchetypes));
      }
      break;
  }
  return problems;
}

export function toRequest(draft: CharacterDraft): CreateCharacterRequest {
  return {
    name: `${draft.firstName.trim()} ${draft.lastName.trim()}`,
    archetype: draft.archetype,
    ageGroup: draft.ageGroup,
    age: draft.age,
    attributes: draft.attributes,
    skills: Object.fromEntries(Object.entries(draft.skills).filter(([, v]) => v > 0)),
    resources: draft.resources,
    talent: draft.talent,
    motivation: draft.motivation.trim(),
    trauma: draft.trauma.trim(),
    darkSecret: draft.darkSecret.trim(),
    relationships: draft.relationships.map((r) => ({ ...r, name: r.name.trim(), text: r.text.trim() })),
    memento: draft.memento.trim(),
    equipment: draft.equipment,
    description: draft.description.trim(),
  };
}

export function pick<T>(list: T[]): T | undefined {
  return list.length ? list[Math.floor(Math.random() * list.length)] : undefined;
}
