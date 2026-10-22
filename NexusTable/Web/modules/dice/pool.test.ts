import rulesJson from "../../../Api/Nexus.Table/Rules/vaesen-rules.json";
import type { Character } from "@/common/api/character";
import type { Roll } from "@/common/api/roll";
import type { VaesenRules } from "@/common/api/rules";
import { canPush, previewPool, pushConditions } from "./pool";

const rules = rulesJson as unknown as VaesenRules;

const character = {
  attributes: { physique: 3, precision: 3, logic: 5, empathy: 4 },
  skills: { learning: 3, agility: 0 },
  conditions: ["frightened", "exhausted"],
} as unknown as Character;

describe("dice pool", () => {
  it("adds attribute, skill and modifiers, and takes a die per Condition of the type", () => {
    const pool = previewPool(rules, character, { skill: "learning", difficulty: -1, bonus: 1, useAdvantage: true });
    expect(pool.parts.map((p) => [p.label, p.dice])).toEqual([
      ["Logic", 5], ["Learning", 3], ["Difficulty", -1], ["Gear and help", 1], ["Advantage", 2], ["Frightened", -1],
    ]);
    expect(pool.size).toBe(9);
  });

  it("always rolls at least one die", () => {
    expect(previewPool(rules, character, { skill: "agility", difficulty: -3, bonus: -3, useAdvantage: false }).size).toBe(1);
  });

  it("supports an attribute test without a skill", () => {
    expect(previewPool(rules, character, { attribute: "empathy", difficulty: 0, bonus: 0, useAdvantage: false }).size).toBe(3);
  });

  it("pushes only failed rolls, for a Condition of the roll's type not yet taken", () => {
    const roll = { attribute: "physique", successes: 0, pushed: false } as Roll;
    expect(canPush(roll)).toBe(true);
    expect(canPush({ ...roll, successes: 1 })).toBe(false);
    expect(pushConditions(rules, character, roll).map((c) => c.id)).toEqual(["battered", "wounded"]);
  });
});
