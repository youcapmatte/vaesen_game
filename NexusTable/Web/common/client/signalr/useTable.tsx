"use client";

import { HubConnectionBuilder, HubConnectionState, LogLevel, type HubConnection } from "@microsoft/signalr";
import { createContext, useContext, useEffect, useRef, useState } from "react";
import type { Character } from "@/common/api/character";
import type { Roll } from "@/common/api/roll";
import { useSecurity } from "../security/SecurityProvider";
import ClientAppsettings from "../settings/ClientAppsettings";

export interface TableEvents {
  societyChanged: (societyId: string) => void;
  characterChanged: (character: Character) => void;
  rollMade: (roll: Roll) => void;
}

type ConnectionStatus = "offline" | "connecting" | "online";

interface TableConnection {
  status: ConnectionStatus;
  connection?: HubConnection;
}

const TableContext = createContext<TableConnection>({ status: "offline" });

/**
 * One SignalR connection to /hubs/table while signed in. The API pushes society, character and roll changes
 * to everyone at the table; components subscribe with useTableEvent.
 */
export function TableConnectionProvider({ children }: { children: React.ReactNode }) {
  const { status: signInStatus, identity, getToken } = useSecurity();
  const [state, setState] = useState<TableConnection>({ status: "offline" });
  const getTokenRef = useRef(getToken);
  getTokenRef.current = getToken;

  useEffect(() => {
    if (signInStatus !== "signedIn") {
      return;
    }

    const connection = new HubConnectionBuilder()
      .withUrl(`${ClientAppsettings.ApiUrl}/hubs/table`, {
        accessTokenFactory: async () => (await getTokenRef.current()) ?? "",
      })
      .withAutomaticReconnect()
      .configureLogging(LogLevel.Warning)
      .build();

    let stopped = false;
    const update = () => setState({
      status: connection.state === HubConnectionState.Connected ? "online" : "connecting",
      connection,
    });
    connection.onreconnecting(update);
    connection.onreconnected(update);
    connection.onclose(() => !stopped && setState({ status: "offline", connection }));

    setState({ status: "connecting", connection });
    connection.start().then(update).catch(() => !stopped && setState({ status: "offline", connection }));

    return () => {
      stopped = true;
      void connection.stop();
    };
  }, [signInStatus, identity]);

  return <TableContext.Provider value={state}>{children}</TableContext.Provider>;
}

export function useTableStatus(): ConnectionStatus {
  return useContext(TableContext).status;
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
