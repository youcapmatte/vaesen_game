import type { ResultData } from "@/common/api/common";
import ClientAppsettings from "../settings/ClientAppsettings";

export enum Http {
  GET = "GET",
  POST = "POST",
  PUT = "PUT",
  PATCH = "PATCH",
  DELETE = "DELETE",
}

/** Thrown by the data hooks when the API answers success: false, so SWR keeps it as the error. */
export class ApiError extends Error {
  constructor(message: string, readonly status: number, readonly code?: string | null) {
    super(message);
  }
}

/**
 * Calls the Nexus Table API. Always resolves to a ResultData, also for network errors and non-JSON answers,
 * so callers only check `success`.
 */
export async function fetcher<T>(path: string, method: Http, token?: string, payload?: unknown): Promise<ResultData<T> & { status: number }> {
  const headers: Record<string, string> = { Accept: "application/json" };
  if (payload !== undefined) {
    headers["Content-Type"] = "application/json";
  }
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  let response: Response;
  try {
    response = await fetch(`${ClientAppsettings.ApiUrl}${path}`, {
      method,
      headers,
      body: payload === undefined ? undefined : JSON.stringify(payload),
    });
  } catch {
    return { success: false, errorMessage: "The Society's messenger never arrived. Is the API running?", errorCode: "network", status: 0 };
  }

  let body: Partial<ResultData<T>> = {};
  try {
    body = await response.json();
  } catch {
    // An empty or HTML body: fall through to the status text.
  }

  if (response.status === 401) {
    return { success: false, errorMessage: "Your session has ended. Sign in again.", errorCode: "unauthorized", status: 401 };
  }

  return {
    success: body.success ?? response.ok,
    result: body.result,
    errorMessage: body.errorMessage || (response.ok ? "" : response.statusText || `Request failed (${response.status})`),
    errorCode: body.errorCode,
    status: response.status,
  };
}
