import type { AccountRole } from "./account";
import type { Character } from "./character";
import type { BaseCore } from "./common";

export interface Society extends BaseCore {
  name: string;
  headquarters: string;
  memberAccountIds: string[];
  mysteryIds: string[];
  /** The case being played: only its title and hook, which the whole table may see. */
  atTable?: MysteryAtTable;
}

export interface MysteryAtTable {
  mysteryId: string;
  title: string;
  /** The letter, article or plea that brings the Society in. */
  hookText: string;
  broughtAt: string;
}

export interface SocietyMember {
  accountId: string;
  displayName: string;
  avatarUrl?: string;
  role: AccountRole;
  character?: Character;
}

export interface SocietyView {
  society: Society;
  members: SocietyMember[];
}

export interface UpdateSocietyRequest {
  name?: string;
  headquarters?: string;
}

export const SocietyApi = {
  society: "/api/society",
};
