import type { Character } from "@/common/api/character";
import type { Roll, RollPoolPart } from "@/common/api/roll";
import type { VaesenRules } from "@/common/api/rules";

export const ADVANTAGE_DICE = 2;
export const MAX_MODIFIER = 3;

export interface PoolChoice {
  skill?: string;
  attribute?: string;
  difficulty: number;
  bonus: number;
  useAdvantage: boolean;
}

export function isPhysicalAttribute(attribute: string) {
  return attribute === "physique" || attribute === "precision";
}

/** The attribute a test uses: the skill's, or the one chosen for an attribute test. */
export function testAttribute(rules: VaesenRules, choice: PoolChoice) {
  return choice.skill ? rules.skills.find((s) => s.id === choice.skill)?.attribute : choice.attribute;
}

/**
 * The pool the API will build (DiceService.BuildPool): attribute, skill, difficulty, bonus, Advantage, and -1 for
 * each Condition of the test's type. The size is at least one die.
 */
export function previewPool(rules: VaesenRules, character: Character, choice: PoolChoice): { parts: RollPoolPart[]; size: number } {
  const attribute = testAttribute(rules, choice);
  if (!attribute) {
    return { parts: [], size: 0 };
  }

  const parts: RollPoolPart[] = [
    { label: rules.attributes.find((a) => a.id === attribute)?.name ?? attribute, dice: character.attributes[attribute] ?? 0 },
  ];
  const skill = rules.skills.find((s) => s.id === choice.skill);
  if (skill) parts.push({ label: skill.name, dice: character.skills[skill.id] ?? 0 });
  if (choice.difficulty) parts.push({ label: "Difficulty", dice: choice.difficulty });
  if (choice.bonus) parts.push({ label: "Gear and help", dice: choice.bonus });
  if (choice.useAdvantage) parts.push({ label: "Advantage", dice: ADVANTAGE_DICE });

  const conditions = isPhysicalAttribute(attribute) ? rules.conditions.physical : rules.conditions.mental;
  for (const condition of conditions) {
    if (character.conditions.includes(condition.id)) parts.push({ label: condition.name, dice: -1 });
  }

  return { parts, size: Math.max(1, parts.reduce((sum, p) => sum + p.dice, 0)) };
}

/** The Conditions a push may cost: those of the roll's type the character doesn't have. Empty means the push breaks them. */
export function pushConditions(rules: VaesenRules, character: Character, roll: Roll) {
  const ofType = isPhysicalAttribute(roll.attribute) ? rules.conditions.physical : rules.conditions.mental;
  return ofType.filter((c) => !character.conditions.includes(c.id));
}

export function canPush(roll: Roll) {
  return !roll.pushed && roll.successes === 0;
}

/** The faces to show: after a push, the pushed dice. */
export function finalDice(roll: Roll) {
  return roll.pushedDice ?? roll.dice;
}
