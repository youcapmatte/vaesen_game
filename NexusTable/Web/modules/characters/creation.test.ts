import rulesJson from "../../../Api/Nexus.Table/Rules/vaesen-rules.json";
import type { VaesenRules } from "@/common/api/rules";
import { attributeBudget, emptyDraft, skillBudget, stepProblems, toRequest, withAgeGroup, withArchetype } from "./creation";

const rules = rulesJson as unknown as VaesenRules;
const academic = rules.archetypes.find((a) => a.id === "academic")!;

function youngAcademic() {
  return withAgeGroup(withArchetype(emptyDraft(), rules, academic), rules, "young");
}

describe("character creation", () => {
  it("starts an archetype at minimum attributes, no skills and the lowest Resources", () => {
    const draft = youngAcademic();
    expect(attributeBudget(rules, draft)).toEqual({ total: 15, spent: 8, remaining: 7 });
    expect(skillBudget(rules, draft)).toEqual({ total: 10, spent: 0, remaining: 10 });
    expect(draft.resources).toBe(4);
    expect(draft.equipment).toHaveLength(academic.equipment.length);
  });

  it("puts the age inside the chosen group", () => {
    const draft = withAgeGroup({ ...youngAcademic(), age: 70 }, rules, "middleAged");
    expect(draft.age).toBeGreaterThanOrEqual(26);
    expect(draft.age).toBeLessThanOrEqual(50);
  });

  it("counts Resources above the starting value as skill points", () => {
    const draft = { ...youngAcademic(), resources: 6, skills: { ...youngAcademic().skills, learning: 3 } };
    expect(skillBudget(rules, draft).spent).toBe(5);
  });

  it("blocks a step until its points are spent", () => {
    const draft = youngAcademic();
    expect(stepProblems(rules, draft, "attributes")).toEqual(["7 attribute points left to spend."]);
    draft.attributes = { physique: 3, precision: 3, logic: 5, empathy: 4 };
    expect(stepProblems(rules, draft, "attributes")).toEqual([]);
  });

  it("refuses an archetype the Society already has", () => {
    expect(stepProblems(rules, youngAcademic(), "archetype", ["academic"])).toEqual(["The Society already has its Academic."]);
  });

  it("builds the API request with the full name and only the skills that have points", () => {
    const draft = { ...youngAcademic(), firstName: " Elin ", lastName: "Gregorius", skills: { ...youngAcademic().skills, learning: 2 } };
    const request = toRequest(draft);
    expect(request.name).toBe("Elin Gregorius");
    expect(request.skills).toEqual({ learning: 2 });
  });
});
