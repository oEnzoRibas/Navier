import {describe,it,expect} from 'vitest';
import {scanHeaders,readField} from '../src/parsers/navierFile';
import {createReader,type RangeReader} from '../src/parsers/rangeReader';
import {parseMetadata} from '../src/parsers/metadata';
import {LocalNavierDataSource} from '../src/data/LocalNavierDataSource';
import {InlineExecutor} from '../src/parsers/workers/WorkerExecutor';
import {ByteLRU} from '../src/data/cache/ByteLRU';
import {derivedDescriptors,deriveField} from '../src/analysis/derived';
import {statistics} from '../src/parsers/navierFile';
import type {FieldData} from '../src/data/types';
import {findStep,moveIndex,parseStep} from '../src/state/timeline';
function bytes(records:{step:bigint;time:number;values:number[]}[]):Uint8Array<ArrayBuffer>{
  const size=8+records.reduce((n,r)=>n+24+r.values.length*8,0),buffer=new ArrayBuffer(size),view=new DataView(buffer);new Uint8Array(buffer).set(new TextEncoder().encode('NAVIER01'));let offset=8;
  for(const r of records){view.setBigUint64(offset,r.step,true);view.setFloat64(offset+8,r.time,true);view.setBigUint64(offset+16,BigInt(r.values.length),true);r.values.forEach((v,i)=>view.setFloat64(offset+24+i*8,v,true));offset+=24+r.values.length*8;}return new Uint8Array(buffer);
}
class TrackedFile extends File {
  reads:[number,number][]=[];
  override arrayBuffer():Promise<ArrayBuffer>{throw new Error('Whole-file allocation forbidden');}
  override slice(begin=0,end=this.size,type?:string):Blob{this.reads.push([begin,end]);return super.slice(begin,end,type);}
}
const raw={nx:2,ny:2,fields:['a','b']};
function fixture(){return new TrackedFile([bytes([{step:0n,time:0,values:[1,2,3,4,10,20,30,40]},{step:100n,time:1,values:[5,6,7,8,50,60,70,80]}])],'test.data');}
async function open(file:File=fixture()){return LocalNavierDataSource.open(file,raw,{},new InlineExecutor());}
describe('NAVIER01 directory',()=>{
  it('valid signature, multiple records, uint64 and duplicate replacement',async()=>{
    const file=new Blob([bytes([{step:18446744073709551615n,time:0,values:[1]},{step:0n,time:1,values:[2]},{step:0n,time:2,values:[3]}])]);
    const directory=await scanHeaders(createReader({kind:'blob',file}),1);expect(directory.map(d=>d.step)).toEqual([0n,18446744073709551615n]);expect(directory[0]?.time).toBe(2);
  });
  it('rejects invalid signature and unsupported versions',async()=>{
    for(const signature of ['NOTFILE!','NAVIER02'])await expect(scanHeaders(createReader({kind:'blob',file:new Blob([signature])}),1)).rejects.toThrow(/signature|version/);
  });
  it('rejects partial headers and payloads',async()=>{
    const valid=bytes([{step:1n,time:0,values:[2,3]}]);
    await expect(scanHeaders(createReader({kind:'blob',file:new Blob([valid.slice(0,15)])}),2)).rejects.toThrow('header truncated');
    await expect(scanHeaders(createReader({kind:'blob',file:new Blob([valid.slice(0,-1)])}),2)).rejects.toThrow('payload incomplete');
  });
  it('rejects metadata mismatches and overflowing counts',async()=>{
    const valid=bytes([{step:1n,time:0,values:[2]}]);await expect(scanHeaders(createReader({kind:'blob',file:new Blob([valid])}),2)).rejects.toThrow('Metadata');
    new DataView(valid.buffer).setBigUint64(24,18446744073709551615n,true);await expect(scanHeaders(createReader({kind:'blob',file:new Blob([valid])}),1)).rejects.toThrow('payload incomplete');
  });
  it('opens a virtual 10 GiB record by reading only 32 bytes',async()=>{
    const count=10*1024**3/8,header=new ArrayBuffer(32),view=new DataView(header);new Uint8Array(header).set(new TextEncoder().encode('NAVIER01'));view.setBigUint64(8,0n,true);view.setFloat64(16,0,true);view.setBigUint64(24,BigInt(count),true);
    const calls:[number,number][]=[];const reader:RangeReader={size:32+count*8,async read(begin,end){calls.push([begin,end]);if(end>32)throw new Error('Payload must stay on disk');return header.slice(begin,end);}};
    const directory=await scanHeaders(reader,count);expect(directory).toHaveLength(1);expect(calls).toEqual([[0,8],[8,32]]);
  });
});
describe('DataSource lazy ownership and cache',()=>{
  it('opening scans headers only; requesting b reads only b',async()=>{
    const file=fixture(),source=await open(file);expect(file.reads).toEqual([[0,8],[8,32],[96,120]]);
    expect(await source.listCheckpoints({begin:1n,end:100n})).toHaveLength(1);expect((await source.getCheckpoint(100n)).time).toBe(1);
    const field=await source.getField(0n,'b');expect([...field.values]).toEqual([10,20,30,40]);expect(file.reads.at(-1)).toEqual([64,96]);await source.close();
  });
  it('cache hit avoids IO and eviction is bounded by bytes',async()=>{
    const file=fixture(),source=await open(file);source.setCacheLimit(60);await source.getField(0n,'a');const reads=file.reads.length;await source.getField(0n,'a');expect(file.reads).toHaveLength(reads);
    await source.getField(100n,'a');expect(source.diagnostics().cacheBytes).toBe(48);expect(source.diagnostics().cacheEntries).toBe(1);await source.getField(0n,'a');expect(file.reads.length).toBe(reads+2);await source.close();expect(source.diagnostics().cacheBytes).toBe(0);
  });
  it('cancelled requests never enter the cache',async()=>{
    const source=await open(),controller=new AbortController();controller.abort();await expect(source.getField(0n,'a',{signal:controller.signal})).rejects.toMatchObject({name:'AbortError'});expect(source.diagnostics().cacheEntries).toBe(0);await source.close();await expect(source.getField(0n,'a')).rejects.toMatchObject({name:'AbortError'});
  });
  it('samples a volume by selected rows without materializing the complete field',async()=>{
    const metadata=parseMetadata({nx:8,ny:8,nz:8,fields:['density']}),field=metadata.fields[0];if(!field)throw Error('Fixture field missing');
    const buffer=bytes([{step:0n,time:0,values:Array.from({length:512},(_,i)=>i)}]),calls:[number,number][]=[];
    const reader:RangeReader={size:buffer.length,async read(begin,end){calls.push([begin,end]);return buffer.buffer.slice(begin,end);}};
    const entry=(await scanHeaders(reader,512))[0];if(!entry)throw Error('Fixture checkpoint missing');calls.length=0;
    const loaded=await readField(reader,entry,field,0,8);expect(loaded.dimensions).toEqual([2,2,2]);expect(loaded.stride).toBe(4);expect(calls).toHaveLength(4);expect(calls.every(([a,b])=>b-a===64)).toBe(true);expect([...loaded.values]).toEqual([0,4,32,36,256,260,288,292]);
  });
  it('LRU retains recent entries, skips oversized values and disposes',()=>{const evicted:number[]=[],cache=new ByteLRU<number>(10,v=>evicted.push(v));cache.set('a',1,5);cache.set('b',2,5);cache.get('a');cache.set('c',3,5);expect(cache.get('b')).toBeUndefined();expect(evicted).toEqual([2]);cache.set('large',9,20);expect(cache.bytes).toBe(10);cache.clear();expect(cache.bytes).toBe(0);});
});
describe('generic derived fields and timeline',()=>{
  it('discovers arbitrary vector groups, hides incomplete curl dependencies',()=>{const m=parseMetadata({nx:2,ny:2,nz:2,fields:['electric_x','electric_y','temperature']});const d=derivedDescriptors(m);expect(d.map(f=>f.id)).toEqual(['magnitude:electric']);});
  it('computes exact magnitude and periodic curl while preserving inputs',()=>{
    const n=8,m=parseMetadata({nx:n,ny:n,fields:['electric_x','electric_y']});const make=(i:number,values:Float64Array):FieldData=>{const field=m.fields[i];if(!field)throw Error('Missing fixture');return {field,values,renderValues:Float32Array.from(values),dimensions:m.dimensions,sourceDimensions:m.dimensions,stride:1,stats:statistics(values)};};
    const u=Float64Array.from({length:n*n},(_,i)=>Math.sin(2*Math.PI*Math.floor(i/n)/n)),v=Float64Array.from({length:n*n},(_,i)=>Math.cos(2*Math.PI*(i%n)/n)),inputs=[make(0,u),make(1,v)],desc=derivedDescriptors(m);
    const mag=desc.find(f=>f.operation==='magnitude'),curl=desc.find(f=>f.operation==='curl');if(!mag||!curl)throw Error('Missing derived fixtures');
    const result=deriveField(mag,inputs,m.spacing,true);expect(result.values[0]).toBe(1);expect(u[0]).toBe(0);
    const c=deriveField(curl,inputs,m.spacing,true);expect(c.values[0]).toBeCloseTo(-Math.sin(2*Math.PI/n),13);
  });
  it('large descriptors need binary search, no per-state UI nodes',()=>{const checkpoints=Array.from({length:100_001},(_,i)=>({step:BigInt(i)*100n,time:i}));expect(findStep(checkpoints,10_000_000n)).toBe(100_000);expect(findStep(checkpoints,4n)).toBe(-1);expect(moveIndex(100_000,checkpoints.length,1,true)).toBe(0);expect(moveIndex(0,10,-5)).toBe(0);expect(parseStep('18446744073709551615')).toBe(18446744073709551615n);expect(()=>parseStep('-1')).toThrow();});
});

