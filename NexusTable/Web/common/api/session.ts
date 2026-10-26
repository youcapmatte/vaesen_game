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
  /** When it happened; for a note, when what it is about happened (when its writer began it). */
  at: string;
  /** Seconds since the session started, on the API's clock: the same for everyone at the table and the Gamemaster. */
  sessionTime: number;
  /** A note sent a while after the moment it is about: when it was sent. */
  sentAt?: string;
  kind: SessionEventKind;
  accountId?: string;
  speaker: string;
  text: string;
  rollId?: string;
  test?: TestRequest;
  hostOnly: boolean;
}

/** The session clock as the log and the Gamemaster write it: "0:41:55". */
export function sessionStamp(seconds: number): string {
  const s = Math.max(0, Math.floor(seconds));
  return `${Math.floor(s / 3600)}:${String(Math.floor((s % 3600) / 60)).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
}

/**
 * The body of a note: its text, and how long ago its writer began it, by this browser's clock. Only the duration
 * crosses, so the API places the note on the session clock right even when this computer's clock is wrong.
 */
export function noteBody(text: string, begunAt: number | undefined, now: number = performance.now()) {
  return { text, secondsAgo: begunAt === undefined ? 0 : Math.max(0, (now - begunAt) / 1000) };
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
