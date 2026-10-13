import {it,expect} from 'vitest';
import {FramePipeline} from '../src/visualization/FramePipeline';
import {preparationKey} from '../src/visualization/preparationKey';
import {defaultSettings} from '../src/visualization/settings';
it('visual adjustments preserve the preparation key; geometry changes invalidate it',()=>{const base=preparationKey('source',0n,'a',defaultSettings);expect(preparationKey('source',0n,'a',{...defaultSettings,slicePosition:.8,projection:'orthographic',pointSize:7,colors:{...defaultSettings.colors,opacity:.2,palette:'thermal',min:-2,max:4}})).toBe(base);expect(preparationKey('source',0n,'a',{...defaultSettings,mode:'streamlines'})).not.toBe(base);expect(preparationKey('source',1n,'a',defaultSettings)).not.toBe(base);});
it('promotes and deduplicates an in-flight prefetch without cancellation',async()=>{const pipeline=new FramePipeline<number>();pipeline.retain(['0','1']);await pipeline.request('0',async()=>0);let finish!:(value:number)=>void,signal!:AbortSignal,calls=0;const pending=pipeline.request('1',s=>{calls++;signal=s;return new Promise(resolve=>finish=resolve);});pipeline.retain(['1','2']);const promoted=pipeline.request('1',async()=>{calls++;return -1;});expect(signal.aborted).toBe(false);expect(promoted).toBe(pending);finish(1);expect(await promoted).toBe(1);expect(calls).toBe(1);expect(await pipeline.request('1',async()=>-1)).toBe(1);pipeline.close();});
it('cancels obsolete preparations on seek and on close',async()=>{const pipeline=new FramePipeline<number>();pipeline.retain(['0']);let signal!:AbortSignal,finish!:(n:number)=>void;const pending=pipeline.request('0',s=>{signal=s;return new Promise(resolve=>finish=resolve);});pipeline.retain(['100']);expect(signal.aborted).toBe(true);finish(0);await expect(pending).rejects.toMatchObject({name:'AbortError'});pipeline.close();});
it('evicts prepared frames by byte budget instead of retaining oversized results',async()=>{const pipeline=new FramePipeline<Uint8Array>(v=>v.byteLength,10);pipeline.retain(['a','b']);await pipeline.request('a',async()=>new Uint8Array(8));await pipeline.request('b',async()=>new Uint8Array(8));let reloads=0;await pipeline.request('a',async()=>{reloads++;return new Uint8Array(8);});expect(reloads).toBe(1);pipeline.close();});

it('switching electric/magnetic lines invalidates geometry but scalar-only views keep their data',()=>{
  const scalar=preparationKey('antenna',10n,'energy_density',defaultSettings);
  expect(preparationKey('antenna',10n,'energy_density',{...defaultSettings,vectorGroup:'magnetic'})).toBe(scalar);
  const electric={...defaultSettings,mode:'streamlines' as const,vectorGroup:'electric'};
  expect(preparationKey('antenna',10n,'energy_density',{...electric,vectorGroup:'magnetic'})).not.toBe(preparationKey('antenna',10n,'energy_density',electric));
});
