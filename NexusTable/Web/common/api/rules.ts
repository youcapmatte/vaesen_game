/** The Vaesen character rules served by GET /api/rules (NexusTable/Api/Nexus.Table/Rules/vaesen-rules.json). */
export interface RuleAttribute { id: string; name: string; description: string }
export interface RuleSkill { id: string; name: string; attribute: string; description: string }
export interface RuleAgeGroup { id: string; name: string; minAge: number; maxAge: number | null; attributePoints: number; skillPoints: number }
export interface RuleCreationLimits { attributeMin: number; attributeMax: number; mainAttributeMax: number; skillMax: number; mainSkillMax: number }
export interface RuleArchetype {
  id: string;
  name: string;
  summary: string;
  mainAttribute: string;
  mainSkill: string;
  talents: string[];
  resources: { min: number; max: number };
  /** One entry per pick; each lists the items to choose between. */
  equipment: string[][];
  names: { first: string[]; last: string[] };
  motivations: string[];
  traumas: string[];
  darkSecrets: string[];
  relationships: string[];
}
export interface RuleTalent { id: string; name: string; archetype: string | null; effect: string }
export interface RuleCondition { id: string; name: string }
export interface RuleResourceLevel { value: number; name: string }
export interface RuleMemento { roll: number; item: string }

export interface VaesenRules {
  attributes: RuleAttribute[];
  skills: RuleSkill[];
  ageGroups: RuleAgeGroup[];
  creationLimits: RuleCreationLimits;
  archetypes: RuleArchetype[];
  talents: RuleTalent[];
  conditions: { physical: RuleCondition[]; mental: RuleCondition[] };
  resourceLevels: RuleResourceLevel[];
  mementos: RuleMemento[];
  dice: { successOn: number; pushing: string; fear: string; notes: string[] };
}

export const RulesApi = {
  rules: "/api/rules",
  memento: "/api/rules/memento",
};
