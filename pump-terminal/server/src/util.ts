export async function fetchJson<T = any>(
  url: string,
  init: RequestInit & { timeoutMs?: number } = {},
): Promise<T> {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), init.timeoutMs ?? 10_000);
  try {
    const res = await fetch(url, { ...init, signal: ctrl.signal });
    const text = await res.text();
    if (!res.ok) throw new Error(`HTTP ${res.status} ${url.split("?")[0]} : ${text.slice(0, 300)}`);
    return (text ? JSON.parse(text) : null) as T;
  } finally {
    clearTimeout(timer);
  }
}

/** Petit cache mémoire avec TTL et déduplication des requêtes en vol. */
export class TtlCache<V> {
  private map = new Map<string, { at: number; value?: V; pending?: Promise<V> }>();
  constructor(private ttlMs: number, private maxEntries = 2000) {}

  async get(key: string, loader: () => Promise<V>, ttlMs = this.ttlMs): Promise<V> {
    const hit = this.map.get(key);
    const now = Date.now();
    if (hit?.pending) return hit.pending;
    if (hit && hit.value !== undefined && now - hit.at < ttlMs) return hit.value;
    const pending = loader()
      .then((value) => {
        this.map.set(key, { at: Date.now(), value });
        return value;
      })
      .catch((err) => {
        if (hit?.value !== undefined) this.map.set(key, hit);
        else this.map.delete(key);
        throw err;
      });
    this.map.set(key, { at: hit?.at ?? 0, value: hit?.value, pending });
    if (this.map.size > this.maxEntries) {
      const first = this.map.keys().next().value;
      if (first !== undefined) this.map.delete(first);
    }
    return pending;
  }
}

/** Exécute des tâches asynchrones avec une concurrence limitée. */
export async function mapLimit<T, R>(items: T[], limit: number, fn: (item: T, i: number) => Promise<R>): Promise<R[]> {
  const out: R[] = new Array(items.length);
  let next = 0;
  const workers = Array.from({ length: Math.min(limit, items.length) }, async () => {
    while (next < items.length) {
      const i = next++;
      out[i] = await fn(items[i], i);
    }
  });
  await Promise.all(workers);
  return out;
}

export const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export function uid(prefix = ""): string {
  return prefix + Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
}

export function dayKey(ts = Date.now()): string {
  return new Date(ts).toISOString().slice(0, 10);
}

export function errMsg(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}
