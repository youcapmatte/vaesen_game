import type { BaseCore } from "./common";

export interface CharacterRelationship {
  characterId?: string;
  name: string;
  text: string;
}

export interface Character extends BaseCore {
  accountId: string;
  societyId: string;
  name: string;
  archetype: string;
  ageGroup: string;
  age: number;
  attributes: Record<string, number>;
  skills: Record<string, number>;
  resources: number;
  talents: string[];
  motivation: string;
  trauma: string;
  darkSecret: string;
  relationships: CharacterRelationship[];
  memento: string;
  equipment: string[];
  description: string;
  conditions: string[];
  physicallyBroken: boolean;
  mentallyBroken: boolean;
  experience: number;
  advantage?: string;
  insightsAndDefects: string[];
  portraitUrl?: string;
  notes: string;
}

export interface CreateCharacterRequest {
  name: string;
  archetype: string;
  ageGroup: string;
  age: number;
  attributes: Record<string, number>;
  skills: Record<string, number>;
  resources: number;
  talent: string;
  motivation: string;
  trauma: string;
  darkSecret: string;
  relationships: CharacterRelationship[];
  memento: string;
  equipment: string[];
  description: string;
  portraitUrl?: string;
}

export type UpdateCharacterRequest = Partial<
  Pick<Character, "motivation" | "darkSecret" | "relationships" | "memento" | "equipment" | "description" | "advantage" | "portraitUrl" | "notes">
>;

export interface SetConditionRequest {
  condition: string;
  active: boolean;
}

export const CharacterApi = {
  characters: "/api/characters",
  character: (id: string) => `/api/characters/${id}`,
  conditions: (id: string) => `/api/characters/${id}/conditions`,
};
