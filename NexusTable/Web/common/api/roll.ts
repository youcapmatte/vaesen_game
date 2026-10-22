import type { Character } from "./character";
import type { BaseCore } from "./common";

export interface RollPoolPart {
  label: string;
  dice: number;
}

export interface Roll extends BaseCore {
  societyId: string;
  accountId: string;
  characterId: string;
  characterName: string;
  skill?: string;
  attribute: string;
  pool: RollPoolPart[];
  poolSize: number;
  dice: number[];
  pushedDice?: number[];
  successes: number;
  pushed: boolean;
  pushCondition?: string;
  reason?: string;
}

export interface RollRequest {
  characterId: string;
  skill?: string;
  attribute?: string;
  difficulty: number;
  bonus: number;
  useAdvantage: boolean;
  reason?: string;
}

export interface PushRollRequest {
  condition: string;
}

export interface RollOutcome {
  roll: Roll;
  character: Character;
}

export const RollApi = {
  rolls: "/api/rolls",
  push: (id: string) => `/api/rolls/${id}/push`,
};
