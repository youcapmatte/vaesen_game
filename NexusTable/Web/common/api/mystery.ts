/**
 * The mystery engine as the host sees it (host only: these are spoilers). Mirrors Vaesen.Framework.Mystery's
 * CasePacket, StoredCase and CaseSummary, and Nexus.Table's MysteryJob.
 */

export type MysteryJobStatus = "running" | "succeeded" | "failed";

export interface MysteryJob {
  jobId: string;
  kind: "mystery" | "world" | "lab";
  status: MysteryJobStatus;
  startedAt: string;
  endedAt?: string | null;
  progress: string[];
  mysteryId?: string | null;
  title?: string | null;
  error?: string | null;
  /** A lab run: how many cases were asked for, each one as it ends, and whether the host asked it to stop. */
  total?: number | null;
  results: LabResult[];
  stopRequested: boolean;
}

export interface LabResult {
  mysteryId?: string | null;
  title?: string | null;
  passed: boolean;
  error?: string | null;
}

/** The most cases one lab run writes. */
export const LabMaxCount = 10;

export type ExportFormat = "markdown" | "json";

export interface MysteryExport {
  fileName: string;
  contentType: string;
  content: string;
}

export interface CaseSummary {
  mysteryId: string;
  societyId: string;
  title: string;
  generatedAt: string;
  levelId: string;
  month: number;
  passed: boolean;
  vaesenIds: string[];
  hookChannelId: string;
  hookGiverType: string;
  status: CaseStatus;
}

/** generated: written, not yet played to its end; rejected: generation failed and it was kept to see why. */
export type CaseStatus = "generated" | "solved" | "failed" | "rejected";

export interface CoherenceIssue {
  code: string;
  severity: "error" | "warning";
  location: string;
  explanation: string;
  suggestedFix?: string | null;
  source: "structural" | "critic";
}

export interface EscalationEvent {
  id: string;
  description: string;
  when: string;
  placeId?: string | null;
  register: string;
  factIds: string[];
  opening: boolean;
}

export interface CaseFact {
  id: string;
  statement: string;
  loadBearing: boolean;
  kind: string;
  pointsTo: string;
  giveaway: boolean;
  deepLore: boolean;
  loreIds: string[];
}

export interface CaseChannel {
  id: string;
  factId: string;
  kind: string;
  sourceCharacterId?: string | null;
  placeId?: string | null;
  affordanceId?: string | null;
  access: { requires: string[] };
  reveal: string;
}

export interface CastMember {
  characterId: string;
  name: string;
  instanceRef: string;
  plotRole: string;
  tier: "significant" | "resident";
  identity?: { age: number; gender: string; occupation: string; rank: string; home: string; appearance: string } | null;
  homePlaceId?: string | null;
  knows: { factId: string; source: string; when: string }[];
  thinks: { belief: string; true: boolean }[];
  wants: { drives: string[]; fears: string[]; guilt?: string | null; exposureBelief: number; permittedActions: { id: string; tier: string; description: string }[] };
  secrets: string[];
  lies: { about: string; story: string }[];
  routine: string;
  voiceNotes: string;
}

export interface CasePacket {
  mysteryId: string;
  societyId: string;
  title: string;
  generatedAt: string;
  models: { creative: string; critic: string };
  brief: { levelId: string; vaesenIds: string[]; month: number; conflictArchetypeId: string; hookChannelId: string; hookGiverType: string; seed: number };
  truth: {
    summary: string;
    reasonKind: string;
    origin?: string | null;
    plotCentre: { kind: string; characterId?: string | null };
    focus: "what" | "whoWhy" | "how";
    provocation: string;
    provocationPlaceId: string;
    hiddenHandMotive?: string | null;
    decoyReading?: string | null;
    escalation: EscalationEvent[];
  };
  hook: { channel: string; hookGiverId: string; text: string; assertedFactIds: string[]; openingEventIds: string[] };
  level: { levelId: string; placesUsed: string[]; sceneDescriptions: Record<string, string> };
  facts: CaseFact[];
  channels: CaseChannel[];
  cast: CastMember[];
  decoys: { threadId: string; description: string; factIds: string[]; clearedByFactIds: string[]; resolution: string }[];
  beats: { id: string; threshold: number; description: string; requires: { place?: string | null; time?: string | null }; emitsFactIds: string[]; actorId?: string | null }[];
  climax: { threshold: number; conditions: { place?: string | null; time?: string | null }; description: string };
  resolutionLevers: {
    id: string;
    description: string;
    solveDepth: "crude" | "deep";
    requiresFactIds: string[];
    consequences: {
      world: string[];
      attitudes: { characterId: string; change: string; delta: number }[];
      fates: { characterId: string; status: string; reason: string }[];
    };
  }[];
  tension: { start: number };
  validation: { passed: boolean; issues: CoherenceIssue[]; runs: number };
}

export interface GenerationLogEntry {
  caseAttempt: number;
  step: string;
  attempt: number;
  model?: string | null;
  inputTokens: number;
  outputTokens: number;
  cacheReadTokens: number;
  latencyMs: number;
  passed: boolean;
  error?: string | null;
  issues: CoherenceIssue[];
}

export interface CaseClosing {
  closedAt: string;
  outcome: "solved" | "failed";
  leverId?: string | null;
  solveDepth?: string | null;
  notes: string;
  changes: string[];
}

export interface StoredCase {
  packet: CasePacket;
  brief: { notes: string[] };
  log: GenerationLogEntry[];
  prompts: { system: string; context: string };
  status: CaseStatus;
  closing?: CaseClosing | null;
}

export interface CloseMysteryRequest {
  outcome: "solved" | "failed";
  leverId?: string | null;
  notes: string;
}

/** The Society's persistent world (Vaesen.Framework.Mystery SocietyWorld). Host only. */
export interface SocietyWorld {
  societyId: string;
  seededAt: string;
  updatedAt: string;
  characters: CharacterInstance[];
  overlay: { mysteryId?: string | null; levelId?: string | null; text: string; at: string }[];
  closedMysteryIds: string[];
  seedLog: string[];
}

export type WorldTier = "canonical" | "core" | "background" | "locationResident";

export interface CharacterInstance {
  id: string;
  definitionId?: string | null;
  name: string;
  tier: WorldTier;
  status: "alive" | "dead" | "departed" | "absent";
  importance: number;
  home: string;
  identity: { age: number; gender: string; occupation: string; rank: string; home: string; appearance: string };
  drives: { id: string; text: string; strength: number }[];
  voice: { register: string; mannerisms: string[]; sampleLines: string[] };
  background: string;
  latentHooks: { id: string; kind: string; text: string }[];
  attitude: number;
  attitudeReason: string;
  relationships: { characterId: string; nature: string; sentiment: number }[];
  memories: { mysteryId?: string | null; when: string; text: string }[];
  knowledge: { mysteryId: string; factId: string; statement: string; source: string }[];
  beliefs: string[];
  mysteryIds: string[];
}

export const WorldApi = {
  world: "/api/world",
  seed: "/api/world/seed",
};

export const TierNames: Record<WorldTier, string> = {
  canonical: "Known to every Society",
  core: "The Society's own circle",
  background: "Faces in the background",
  locationResident: "Met on the road",
};

/** -3 to +3 as words. */
export function attitudeName(attitude: number) {
  return ["Hostile", "Resentful", "Wary", "Indifferent", "Friendly", "Loyal", "Devoted"][Math.max(-3, Math.min(3, attitude)) + 3];
}

export const MysteryApi = {
  list: "/api/mysteries",
  labList: "/api/mysteries?lab=true",
  generate: "/api/mysteries/generate",
  lab: "/api/mysteries/lab",
  stop: (jobId: string) => `/api/mysteries/jobs/${jobId}/stop`,
  export: "/api/mysteries/export",
  table: (mysteryId: string) => `/api/mysteries/${mysteryId}/table`,
  clearTable: "/api/mysteries/table",
  currentJob: "/api/mysteries/jobs/current",
  job: (jobId: string) => `/api/mysteries/jobs/${jobId}`,
  case: (mysteryId: string) => `/api/mysteries/${mysteryId}`,
  report: (mysteryId: string) => `/api/mysteries/${mysteryId}/report`,
  close: (mysteryId: string) => `/api/mysteries/${mysteryId}/close`,
};

/** The steps generation reports progress for, in order. */
export const GenerationSteps = [
  { id: "world", label: "Seeding the Society's world (first time only)" },
  { id: "assignment", label: "Choosing the place and the creature" },
  { id: "conflict", label: "The conflict" },
  { id: "escalation", label: "What has happened" },
  { id: "humans", label: "The people" },
  { id: "clues", label: "The clues and the hook" },
  { id: "critic", label: "The critic's review" },
] as const;

export const MonthNames = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];

export const FocusNames: Record<string, string> = {
  what: "What is it?",
  whoWhy: "Who, and why?",
  how: "How to end it?",
};

export const ReasonNames: Record<string, string> = {
  provokedOnly: "Provoked",
  selfOriginatingGrievance: "Its own grievance",
  domainViolated: "Domain violated",
  bargainInvited: "A bargain",
  affliction: "An affliction",
  none: "No reason",
};

/** "level-forsby-october" as "Forsby". */
export function levelName(levelId: string) {
  const parts = levelId.replace(/^level-/, "").split("-");
  const name = parts.length > 1 ? parts.slice(0, -1).join(" ") : parts[0];
  return name.charAt(0).toUpperCase() + name.slice(1);
}
