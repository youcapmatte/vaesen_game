import type { BaseCore } from "./common";
import type { RollPoolPart } from "./roll";

/** One evening of play on a mystery; the next one resumes from the play state this one ended with. */
export interface PlaySession extends BaseCore {
  societyId: string;
  mysteryId: string;
  mysteryTitle: string;
  number: number;
  startedAt: string;
  endedAt?: string;
  /** The host's view only; players get an empty one. */
  state: PlayState;
  /** Tests the Gamemaster asked for that nobody has rolled yet. */
  pendingTests: TestRequest[];
}

/** Where a mystery stands, in the case file's ids: written by the Gamemaster's scribe. */
export interface PlayState {
  summary: string;
  scene: string;
  factsFound: { factId: string; how: string; session: number }[];
  channelsUsed: string[];
  peopleMet: { characterId: string; name: string; note: string }[];
  beatsFired: string[];
  deedsDone: string[];
  openThreads: string[];
  updatedAt?: string;
}

/** A test the Gamemaster asks for: who, what, how hard, the sixes it needs and the dice the character has. */
export interface TestRequest {
  id: string;
  characterId: string;
  characterName: string;
  skill?: string;
  attribute: string;
  difficulty: number;
  successesNeeded: number;
  reason?: string;
  pool: RollPoolPart[];
  poolSize: number;
  at: string;
}

export type SessionEventKind =
  | "sessionStarted" | "sessionEnded" | "playerSpoke" | "gmSaid" | "roll" | "testRequested" | "note" | "gmJoined" | "gmLeft" | "stateUpdated";

/** One line of the session log. */
export interface SessionEvent extends BaseCore {
  sessionId: string;
  mysteryId: string;
  at: string;
  kind: SessionEventKind;
  accountId?: string;
  speaker: string;
  text: string;
  rollId?: string;
  test?: TestRequest;
  hostOnly: boolean;
}

export const SessionApi = {
  current: "/api/sessions/current",
  start: "/api/sessions/start",
  end: (id: string) => `/api/sessions/${id}/end`,
  notes: "/api/sessions/current/notes",
  log: (id: string) => `/api/sessions/${id}/log`,
  forMystery: (mysteryId: string) => `/api/sessions/mystery/${mysteryId}`,
  exportMystery: (mysteryId: string, sessionId?: string) =>
    `/api/sessions/mystery/${mysteryId}/export${sessionId ? `?sessionId=${encodeURIComponent(sessionId)}` : ""}`,
};
