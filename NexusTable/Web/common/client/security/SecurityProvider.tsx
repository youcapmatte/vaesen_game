"use client";

import { getAccessToken } from "@auth0/nextjs-auth0";
import type { User } from "@auth0/nextjs-auth0/types";
import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from "react";
import { AccountApi, type DevLoginResult } from "@/common/api/account";
import { fetcher, Http } from "../core/Fetcher";

const DEV_TOKEN_KEY = "nexus.devToken";

type Mode = "dev" | "auth0";

interface StoredDevToken {
  token: string;
  expiresAt: number;
  name: string;
}

export interface SecurityState {
  status: "loading" | "signedOut" | "signedIn";
  mode?: Mode;
  /** A valid access token for the API (refreshed by Auth0 when needed). */
  getToken: () => Promise<string | undefined>;
  /** Changes whenever the signed-in identity changes, so data hooks refetch. */
  identity?: string;
  auth0Enabled: boolean;
  devLoginEnabled: boolean;
  /** Resolves to an error message, or undefined when signed in. */
  signInDev: (playerId: string) => Promise<string | undefined>;
  signOut: () => void;
}

const SecurityContext = createContext<SecurityState | undefined>(undefined);

/**
 * The dev token lives per tab (sessionStorage), so several tabs can be different players while testing; a new
 * tab starts as whoever signed in last (localStorage).
 */
function readDevToken(): StoredDevToken | undefined {
  try {
    const raw = window.sessionStorage.getItem(DEV_TOKEN_KEY) ?? window.localStorage.getItem(DEV_TOKEN_KEY);
    const stored = raw ? (JSON.parse(raw) as StoredDevToken) : undefined;
    if (!raw || !stored || stored.expiresAt <= Date.now() + 60_000) {
      return undefined;
    }
    // Pin this tab to that player, whoever later signs in elsewhere.
    window.sessionStorage.setItem(DEV_TOKEN_KEY, raw);
    return stored;
  } catch {
    return undefined;
  }
}

/**
 * Who is signed in, and their API token. Two ways in:
 *   - Auth0 with the Discord connection (when the server has AUTH0_* configured): the server layout passes the
 *     session's user; tokens come from /auth/access-token.
 *   - The dev login (NEXT_PUBLIC_DEV_LOGIN, API in Development): a seed player's token kept in localStorage.
 */
export function SecurityProvider({ auth0Enabled, devLoginEnabled, user, children }: {
  auth0Enabled: boolean;
  user?: User;
  devLoginEnabled: boolean;
  children: React.ReactNode;
}) {
  const [status, setStatus] = useState<SecurityState["status"]>("loading");
  const [mode, setMode] = useState<Mode | undefined>();
  const [identity, setIdentity] = useState<string | undefined>();
  const devToken = useRef<StoredDevToken | undefined>(undefined);
  const profileSynced = useRef<string | undefined>(undefined);

  useEffect(() => {
    if (mode === "dev") {
      return;
    }

    const stored = devLoginEnabled ? readDevToken() : undefined;
    if (stored) {
      devToken.current = stored;
      setMode("dev");
      setIdentity(`dev:${stored.name}`);
      setStatus("signedIn");
      return;
    }

    if (!auth0Enabled) {
      setStatus("signedOut");
      return;
    }

    if (user) {
      setMode("auth0");
      setIdentity(`auth0:${user.sub}`);
      setStatus("signedIn");
    } else {
      setStatus("signedOut");
    }
  }, [auth0Enabled, devLoginEnabled, user, mode]);

  const getToken = useCallback(async () => {
    if (mode === "dev") {
      return devToken.current?.token;
    }
    if (mode === "auth0") {
      try {
        return await getAccessToken();
      } catch {
        setStatus("signedOut");
        return undefined;
      }
    }
    return undefined;
  }, [mode]);

  // Access tokens carry no profile, so send the Discord name and avatar from the session once per sign-in.
  useEffect(() => {
    if (mode !== "auth0" || !user?.sub || profileSynced.current === user.sub) {
      return;
    }
    profileSynced.current = user.sub;
    void getToken().then((token) =>
      fetcher(AccountApi.profile, Http.PUT, token, { displayName: user.name ?? user.nickname, avatarUrl: user.picture }));
  }, [mode, user, getToken]);

  const signInDev = useCallback(async (playerId: string) => {
    const result = await fetcher<DevLoginResult>(AccountApi.devLogin, Http.POST, undefined, { playerId });
    if (!result.success || !result.result) {
      return result.errorMessage || "Could not sign in.";
    }
    const stored: StoredDevToken = {
      token: result.result.accessToken,
      expiresAt: Date.now() + result.result.expiresIn * 1000,
      name: result.result.name,
    };
    try {
      window.sessionStorage.setItem(DEV_TOKEN_KEY, JSON.stringify(stored));
      window.localStorage.setItem(DEV_TOKEN_KEY, JSON.stringify(stored));
    } catch {
      // Private window: the sign-in lasts until the tab closes.
    }
    devToken.current = stored;
    setMode("dev");
    setIdentity(`dev:${playerId}`);
    setStatus("signedIn");
    return undefined;
  }, []);

  const signOut = useCallback(() => {
    try {
      window.sessionStorage.removeItem(DEV_TOKEN_KEY);
      window.localStorage.removeItem(DEV_TOKEN_KEY);
    } catch {
      // Nothing stored.
    }
    devToken.current = undefined;
    if (mode === "auth0") {
      window.location.href = "/auth/logout";
      return;
    }
    setMode(undefined);
    setIdentity(undefined);
    setStatus("signedOut");
  }, [mode]);

  const value = useMemo<SecurityState>(
    () => ({ status, mode, getToken, identity, auth0Enabled, devLoginEnabled, signInDev, signOut }),
    [status, mode, getToken, identity, auth0Enabled, devLoginEnabled, signInDev, signOut],
  );

  return <SecurityContext.Provider value={value}>{children}</SecurityContext.Provider>;
}

export function useSecurity(): SecurityState {
  const context = useContext(SecurityContext);
  if (!context) {
    throw new Error("useSecurity must be used inside SecurityProvider.");
  }
  return context;
}
