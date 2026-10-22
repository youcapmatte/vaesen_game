"use client";

import { useCallback } from "react";
import useSWR, { type SWRConfiguration } from "swr";
import type { ResultData } from "@/common/api/common";
import { useSecurity } from "../security/SecurityProvider";
import { ApiError, fetcher, Http } from "./Fetcher";

/**
 * Reads from the API with SWR, signed in as the current user. Pass null to wait (e.g. for an id).
 * The key includes the identity, so switching players never shows the previous player's data.
 *
 *   const { data: society, mutate } = useApiData<SocietyView>(SocietyApi.society);
 */
export function useApiData<T>(path: string | null, options?: SWRConfiguration<T, ApiError> & { anonymous?: boolean }) {
  const { status, identity, getToken } = useSecurity();
  const ready = options?.anonymous || status === "signedIn";
  return useSWR<T, ApiError, [string, string] | null>(
    path && ready ? [path, identity ?? "anonymous"] : null,
    async ([url]) => {
      const result = await fetcher<T>(url, Http.GET, options?.anonymous ? undefined : await getToken());
      if (!result.success) {
        throw new ApiError(result.errorMessage, result.status, result.errorCode);
      }
      return result.result as T;
    },
    { revalidateOnFocus: false, ...options },
  );
}

/**
 * Changes something through the API. Resolves to the ResultData; never throws.
 *
 *   const call = useApi();
 *   const result = await call<Character>(Http.POST, CharacterApi.characters, request);
 */
export function useApi() {
  const { getToken } = useSecurity();
  return useCallback(
    async <T,>(method: Http, path: string, payload?: unknown): Promise<ResultData<T>> =>
      fetcher<T>(path, method, await getToken(), payload),
    [getToken],
  );
}

export { Http };
