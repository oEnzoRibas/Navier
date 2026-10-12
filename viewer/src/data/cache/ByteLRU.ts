export class ByteLRU<T> {
  private entries = new Map<string, {value: T; bytes: number}>();
  private used = 0;
  constructor(private limit: number, private onEvict?: (value: T) => void) { this.setLimit(limit); }
  get bytes(): number { return this.used; }
  get budget(): number { return this.limit; }
  get size(): number { return this.entries.size; }
  get(key: string): T | undefined {
    const entry = this.entries.get(key); if (!entry) return undefined;
    this.entries.delete(key); this.entries.set(key, entry); return entry.value;
  }
  set(key: string, value: T, bytes: number): void {
    this.delete(key);
    if (bytes > this.limit) return; // One oversized result is usable, never retained.
    this.entries.set(key, {value, bytes}); this.used += bytes; this.trim();
  }
  delete(key: string): void {
    const entry = this.entries.get(key); if (!entry) return;
    this.entries.delete(key); this.used -= entry.bytes; this.onEvict?.(entry.value);
  }
  setLimit(bytes: number): void {
    if (!Number.isFinite(bytes) || bytes < 0) throw new Error('Invalid cache budget');
    this.limit = Math.floor(bytes); this.trim();
  }
  clear(): void { for (const key of this.entries.keys()) this.delete(key); }
  private trim(): void { while (this.used > this.limit) { const oldest = this.entries.keys().next().value; if (oldest === undefined) break; this.delete(oldest); } }
}
