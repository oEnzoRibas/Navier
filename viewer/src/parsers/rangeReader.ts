import {checkAbort} from '../data/errors';
export interface RangeReader { readonly size: number; read(begin: number, end: number, signal?: AbortSignal): Promise<ArrayBuffer> }
export type ReaderSpec = {kind:'blob'; file: Blob} | {kind:'http'; url:string; size:number};
export function createReader(spec: ReaderSpec, onRead?: (bytes: number) => void): RangeReader {
  return {size: spec.kind === 'blob' ? spec.file.size : spec.size,
    async read(begin, end, signal) {
      checkAbort(signal);
      const size = spec.kind === 'blob' ? spec.file.size : spec.size;
      if (!Number.isSafeInteger(begin) || !Number.isSafeInteger(end) || begin < 0 || end < begin || end > size) throw new Error('Range outside file bounds');
      let buffer: ArrayBuffer;
      if (spec.kind === 'blob') buffer = await spec.file.slice(begin, end).arrayBuffer();
      else {
        const response = await fetch(spec.url, {headers: {Range:`bytes=${begin}-${end-1}`}, signal});
        if (response.status !== 206 || response.headers.get('Content-Range') !== `bytes ${begin}-${end-1}/${size}`) {
          await response.body?.cancel(); throw new Error('Server does not support verified byte-range reads (HTTP 206 required)');
        }
        buffer = await response.arrayBuffer(); // Only the validated partial response.
      }
      checkAbort(signal);
      if (buffer.byteLength !== end-begin) throw new Error('Incomplete range response / field payload truncated');
      onRead?.(buffer.byteLength); return buffer;
    }};
}
