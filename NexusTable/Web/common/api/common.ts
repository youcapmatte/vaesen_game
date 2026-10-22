/** The envelope every API response uses: { success, result, errorMessage, errorCode }. */
export interface ResultData<T> {
  success: boolean;
  result?: T;
  errorMessage: string;
  errorCode?: string | null;
}

/** Fields every stored model has (Vaesen.Framework.Core BaseCore). */
export interface BaseCore {
  id: string;
  createdAt: string;
  updatedAt: string;
  version: number;
}
