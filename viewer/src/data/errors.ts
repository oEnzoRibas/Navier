export function abortError(): DOMException { return new DOMException('Request cancelled', 'AbortError'); }
export function checkAbort(signal?: AbortSignal): void { if (signal?.aborted) throw abortError(); }
export function isAbort(error: unknown): boolean { return error instanceof Error && error.name === 'AbortError'; }
export function errorMessage(error: unknown): string { return error instanceof Error ? error.message : String(error); }
