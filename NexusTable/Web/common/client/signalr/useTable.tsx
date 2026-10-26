"use client";

import { HubConnectionBuilder, HubConnectionState, LogLevel, type HubConnection } from "@microsoft/signalr";
import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from "react";
import { type Account, AccountApi } from "@/common/api/account";
import type { Character } from "@/common/api/character";
import type { Roll } from "@/common/api/roll";
import type { PlaySession, SessionEvent } from "@/common/api/session";
import { useApiData } from "../core/Api";
import { useSecurity } from "../security/SecurityProvider";
import ClientAppsettings from "../settings/ClientAppsettings";

export interface PresenceEntry {
  accountId: string;
  talkingToGm: boolean;
}

export interface TableEvents {
  societyChanged: (societyId: string) => void;
  characterChanged: (character: Character) => void;
  rollMade: (roll: Roll) => void;
  presenceChanged: (presence: PresenceEntry[]) => void;
  /** A session started or ended, or its pending tests changed (no play state). */
  sessionChanged: (session: PlaySession) => void;
  /** A new line in the session log. */
  sessionEvent: (event: SessionEvent) => void;
}

type ConnectionStatus = "offline" | "connecting" | "online";

interface TableConnection {
  status: ConnectionStatus;
  connection?: HubConnection;
  /** Who is connected right now. */
  presence: PresenceEntry[];
  setPushToTalk: (active: boolean) => void;
}

const TableContext = createContext<TableConnection>({ status: "offline", presence: [], setPushToTalk: () => {} });

/**
 * One SignalR connection to /hubs/table while signed in. The API pushes society, character, roll and presence
 * changes to everyone at the table; components subscribe with useTableEvent.
 */
export function TableConnectionProvider({ children }: { children: React.ReactNode }) {
  const { status: signInStatus, identity, getToken } = useSecurity();
  // Only someone the host has let in gets the table (the hub turns others away).
  const { data: me } = useApiData<Account>(AccountApi.me);
  const approved = me?.isApproved === true;
  const [status, setStatus] = useState<ConnectionStatus>("offline");
  const [connection, setConnection] = useState<HubConnection>();
  const [presence, setPresence] = useState<PresenceEntry[]>([]);
  const getTokenRef = useRef(getToken);
  getTokenRef.current = getToken;

  useEffect(() => {
    if (signInStatus !== "signedIn" || !approved) {
      return;
    }

    const hub = new HubConnectionBuilder()
      .withUrl(`${ClientAppsettings.ApiUrl}/hubs/table`, {
        accessTokenFactory: async () => (await getTokenRef.current()) ?? "",
      })
      .withAutomaticReconnect()
      .configureLogging(LogLevel.Warning)
      .build();

    let stopped = false;
    const refresh = async () => {
      setStatus(hub.state === HubConnectionState.Connected ? "online" : "connecting");
      if (hub.state === HubConnectionState.Connected) {
        setPresence(await hub.invoke<PresenceEntry[]>("GetPresence"));
      }
    };
    hub.on("presenceChanged", (entries: PresenceEntry[]) => setPresence(entries));
    hub.onreconnecting(() => setStatus("connecting"));
    hub.onreconnected(() => void refresh());
    hub.onclose(() => !stopped && setStatus("offline"));

    setConnection(hub);
    setStatus("connecting");
    void (async () => {
      try {
        // The hub knows people by their account, which the first /api/me (above) created.
        await hub.start();
        await refresh();
      } catch {
        if (!stopped) {
          setStatus("offline");
        }
      }
    })();

    return () => {
      stopped = true;
      setPresence([]);
      void hub.stop();
    };
  }, [signInStatus, identity, approved]);

  const setPushToTalk = useCallback((active: boolean) => {
    if (connection?.state === HubConnectionState.Connected) {
      void connection.invoke("SetPushToTalk", active);
    }
  }, [connection]);

  const value = useMemo(() => ({ status, connection, presence, setPushToTalk }), [status, connection, presence, setPushToTalk]);
  return <TableContext.Provider value={value}>{children}</TableContext.Provider>;
}

export function useTableStatus(): ConnectionStatus {
  return useContext(TableContext).status;
}

export function useTablePresence() {
  const { presence, setPushToTalk } = useContext(TableContext);
  return { presence, setPushToTalk };
}

/** Calls handler for every pushed event of that name while the component is mounted. */
export function useTableEvent<K extends keyof TableEvents>(name: K, handler: TableEvents[K]) {
  const { connection } = useContext(TableContext);
  const handlerRef = useRef(handler);
  handlerRef.current = handler;

  useEffect(() => {
    if (!connection) {
      return;
    }
    const listener = (...args: unknown[]) => (handlerRef.current as (...a: unknown[]) => void)(...args);
    connection.on(name, listener);
    return () => connection.off(name, listener);
  }, [connection, name]);
}
