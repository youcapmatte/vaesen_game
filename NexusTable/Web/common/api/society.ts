import type { AccountRole } from "./account";
import type { Character } from "./character";
import type { BaseCore } from "./common";

export interface Society extends BaseCore {
  name: string;
  headquarters: string;
  memberAccountIds: string[];
  mysteryIds: string[];
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
