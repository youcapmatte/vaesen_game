"use client";

import { AccountApi, type Account } from "@/common/api/account";
import { RulesApi, type VaesenRules } from "@/common/api/rules";
import { SocietyApi, type SocietyView } from "@/common/api/society";
import { useApiData } from "../core/Api";
import { useTableEvent } from "../signalr/useTable";

/** The Vaesen rules; they never change while the app runs. */
export function useRules() {
  return useApiData<VaesenRules>(RulesApi.rules, { anonymous: true, revalidateIfStale: false });
}

/** The signed-in account (created and joined to the Society on first call). */
export function useMe() {
  return useApiData<Account>(AccountApi.me);
}

/** The Society with members and characters, kept current by the table hub. */
export function useSociety() {
  const society = useApiData<SocietyView>(SocietyApi.society);
  useTableEvent("societyChanged", () => void society.mutate());
  useTableEvent("characterChanged", (character) =>
    void society.mutate(
      (current) => current && {
        ...current,
        members: current.members.map((m) => (m.character?.id === character.id ? { ...m, character } : m)),
      },
      { revalidate: false },
    ));
  return society;
}

/** Name lookups for ids in the rules. */
export function ruleNames(rules: VaesenRules | undefined) {
  const find = <T extends { id: string; name: string }>(list: T[] | undefined, id: string | undefined) =>
    list?.find((x) => x.id === id)?.name ?? id ?? "";
  return {
    archetype: (id?: string) => find(rules?.archetypes, id),
    attribute: (id?: string) => find(rules?.attributes, id),
    skill: (id?: string) => find(rules?.skills, id),
    talent: (id?: string) => find(rules?.talents, id),
    condition: (id?: string) => find([...(rules?.conditions.physical ?? []), ...(rules?.conditions.mental ?? [])], id),
    ageGroup: (id?: string) => find(rules?.ageGroups, id),
    resources: (value: number) => rules?.resourceLevels.find((r) => r.value === value)?.name ?? String(value),
  };
}
