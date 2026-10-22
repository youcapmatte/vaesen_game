import type { BaseCore } from "./common";

export type AccountRole = "Player" | "Host";

export interface Account extends BaseCore {
  subject: string;
  displayName: string;
  avatarUrl?: string;
  role: AccountRole;
  activeCharacterId?: string;
}

export interface UpdateProfileRequest {
  displayName?: string;
  avatarUrl?: string;
}

export interface DevPlayer {
  id: string;
  name: string;
  isHost: boolean;
}

export interface DevLoginResult {
  accessToken: string;
  expiresIn: number;
  subject: string;
  name: string;
}

export const AccountApi = {
  me: "/api/me",
  profile: "/api/me/profile",
  devPlayers: "/api/dev/players",
  devLogin: "/api/dev/login",
};
