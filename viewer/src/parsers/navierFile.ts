import type {CheckpointDescriptor, Dimensions, FieldData, FieldDescriptor, LoadProgress} from '../data/types';
import type {RangeReader} from './rangeReader';
import {checkAbort} from '../data/errors';
export interface DirectoryEntry extends CheckpointDescriptor { offset: number; count: number }
export async function scanHeaders(reader: RangeReader, expectedCount: number, signal?: AbortSignal, progress?: (p:LoadProgress)=>void): Promise<DirectoryEntry[]> {
  if (reader.size < 8) throw new Error('Navier signature truncated');
  const magic = new TextDecoder().decode(await reader.read(0, 8, signal));
  if (magic !== 'NAVIER01') throw new Error(magic.startsWith('NAVIER') ? 'Unsupported Navier file version' : 'Invalid Navier signature');
  const latest = new Map<bigint, DirectoryEntry>(); let offset = 8, records = 0;
  while (offset < reader.size) {
    checkAbort(signal);
    if (reader.size-offset < 24) throw new Error(`Checkpoint header truncated at byte ${offset}`);
    const view = new DataView(await reader.read(offset, offset+24, signal));
    const step = view.getBigUint64(0,true), time = view.getFloat64(8,true), count64 = view.getBigUint64(16,true);
    if (!Number.isFinite(time)) throw new Error(`Nonfinite checkpoint time at step ${step}`);
    if (count64 > BigInt(Math.floor((reader.size-offset-24)/8))) throw new Error(`Checkpoint payload incomplete at step ${step}`);
    if (count64 !== BigInt(expectedCount)) throw new Error(`Metadata does not match payload at step ${step}: expected ${expectedCount} values, found ${count64}`);
    const count = Number(count64); latest.set(step,{step,time,offset,count}); offset += 24+count*8; records++;
    if (records % 128 === 0) {progress?.({phase:'scanning',bytesRead:8+records*24,totalBytes:reader.size,completed:records}); await new Promise<void>(resolve => setTimeout(resolve,0));}
  }
  progress?.({phase:'scanning',bytesRead:8+records*24,totalBytes:reader.size,completed:records});
  return [...latest.values()].sort((a,b)=>a.step < b.step ? -1 : a.step>b.step ? 1:0);
}
export function statistics(values: Float32Array | Float64Array) {
  let min=Infinity,max=-Infinity,sum=0,finiteCount=0,invalidCount=0;
  for (const v of values) {if (!Number.isFinite(v)) {invalidCount++;continue;} min=Math.min(min,v);max=Math.max(max,v);sum+=v;finiteCount++;}
  return {min: finiteCount?min:0,max:finiteCount?max:0,mean:finiteCount?sum/finiteCount:0,finiteCount,invalidCount};
}
export function planSampling(dimensions:Dimensions,maxSamples?:number): {stride:number;dimensions:Dimensions;cells:number} {
  const total=dimensions[0]*dimensions[1]*dimensions[2];
  let stride=1;
  if (maxSamples !== undefined) {
    if (!Number.isFinite(maxSamples) || maxSamples < 1) throw new Error('Invalid visualization sample budget');
    const activeDimensions=dimensions.filter(n=>n>1).length;
    stride=Math.max(1,Math.ceil((total/maxSamples)**(1/Math.max(activeDimensions,1))));
    while(dimensions.reduce((a,n)=>a*Math.ceil(n/stride),1)>maxSamples)stride++;
  }
  const sampled=dimensions.map(n=>Math.ceil(n/stride)) as unknown as Dimensions;
  return {stride,dimensions:sampled,cells:sampled[0]*sampled[1]*sampled[2]};
}
export async function readField(reader:RangeReader, entry:DirectoryEntry, field:FieldDescriptor, index:number, maxSamples?:number, signal?:AbortSignal, progress?:(p:LoadProgress)=>void):Promise<FieldData> {
  const original=field.dimensions, sourceCells=original[0]*original[1]*original[2], offset=entry.offset+24+index*sourceCells*8;
  const plan=planSampling(original,maxSamples);
  if(plan.cells*12>192*1024*1024)throw new Error('Selected field exceeds the 192 MiB per-field budget. Choose reduced visualization quality.');
  let values:Float64Array;
  if(plan.stride===1){
    const buffer=await reader.read(offset,offset+sourceCells*8,signal);
    values=new Float64Array(sourceCells);const view=new DataView(buffer);
    for(let i=0;i<values.length;i++)values[i]=view.getFloat64(i*8,true);
    progress?.({phase:'loading',bytesRead:buffer.byteLength});
  }else{
    values=new Float64Array(plan.cells);let output=0,bytesRead=0,rows=0;
    for(let z=0;z<original[2];z+=plan.stride)for(let y=0;y<original[1];y+=plan.stride){
      checkAbort(signal);const rowOffset=offset+(z*original[1]+y)*original[0]*8;
      const view=new DataView(await reader.read(rowOffset,rowOffset+original[0]*8,signal));
      bytesRead+=view.byteLength;for(let x=0;x<original[0];x+=plan.stride)values[output++]=view.getFloat64(x*8,true);
      if(++rows%128===0)progress?.({phase:'loading',bytesRead});
    }
    progress?.({phase:'loading',bytesRead});
  }
  checkAbort(signal);
  return {field,values,renderValues:Float32Array.from(values),dimensions:plan.dimensions,sourceDimensions:original,stride:plan.stride,stats:statistics(values)};
}
