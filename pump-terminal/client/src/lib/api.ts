import { useSettings } from "./settings";

export class ApiError extends Error {
  constructor(message: string, public status: number) {
    super(message);
  }
}

export async function api<T = any>(path: string, init: RequestInit = {}): Promise<T> {
  const token = useSettings.getState().accessToken;
  const headers: Record<string, string> = { ...(init.headers as Record<string, string>) };
  if (init.body && !headers["content-type"]) headers["content-type"] = "application/json";
  if (token) headers["x-access-token"] = token;
  const res = await fetch(path, { ...init, headers });
  const text = await res.text();
  let data: any = null;
  try {
    data = text ? JSON.parse(text) : null;
  } catch {
    data = { error: text };
  }
  if (!res.ok) throw new ApiError(data?.error ?? `Erreur HTTP ${res.status}`, res.status);
  return data as T;
}

export const post = <T = any>(path: string, body?: unknown) =>
  api<T>(path, { method: "POST", body: JSON.stringify(body ?? {}) });
export const put = <T = any>(path: string, body?: unknown) =>
  api<T>(path, { method: "PUT", body: JSON.stringify(body ?? {}) });
export const del = <T = any>(path: string) => api<T>(path, { method: "DELETE" });
