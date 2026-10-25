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
  /** "on" when the Gamemaster moves the world after a silence. */
  nudgeAttribute: "nexus.gm.nudge",
} as const;

/**
 * end_session: the Gamemaster closes the evening and its scribe brings the play state up to date. move_on: it frames
 * the next scene, lets an event happen, or brings a missed clue another way. nudge_on/off: it moves the world after
 * a silence at the table.
 */
export type GmControl = "pause" | "resume" | "repeat" | "recap" | "move_on" | "nudge_on" | "nudge_off" | "end_session" | "leave";
