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
  /** Sixes needed to succeed, as the Gamemaster set it: usually one. */
  successesNeeded?: number;
  /** The Gamemaster's test this roll answers. */
  testId?: string;
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
  successesNeeded?: number;
  testId?: string;
}

/** Sixes needed for a roll (old rolls have none: one). */
export const sixesNeeded = (roll: Pick<Roll, "successesNeeded">) => Math.max(1, roll.successesNeeded ?? 1);

export const succeeded = (roll: Pick<Roll, "successes" | "successesNeeded">) => roll.successes >= sixesNeeded(roll);

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
