/** The voice call (LiveKit). */
export interface CallJoin {
  url: string;
  token: string;
  room: string;
  identity: string;
}

/** What the API puts in each participant's LiveKit metadata. */
export interface CallParticipantMetadata {
  accountId: string;
  displayName: string;
  role: "Player" | "Host";
  characterId?: string;
  characterName?: string;
}

export const CallApi = {
  join: "/api/call/join",
  gamemaster: "/api/call/gamemaster",
};

/** The GM agent's RPC methods and attributes (NexusTable/GmAgent/agent.py). */
export const GmProtocol = {
  startTurn: "start_turn",
  endTurn: "end_turn",
  cancelTurn: "cancel_turn",
  control: "gm_control",
  rollTopic: "nexus.roll",
  /** A note typed at the table: the agent reads it back from the API by its event id. */
  noteTopic: "nexus.note",
  stateAttribute: "nexus.gm.state",
  latencyAttribute: "nexus.gm.latencyMs",
} as const;

/** end_session: the Gamemaster closes the evening and its scribe brings the play state up to date. */
export type GmControl = "pause" | "resume" | "repeat" | "recap" | "end_session" | "leave";
