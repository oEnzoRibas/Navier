import {describe,it,expect,vi} from 'vitest';
import {createReader} from '../src/parsers/rangeReader';
import {parseMetadata} from '../src/parsers/metadata';
import {executeTask} from '../src/parsers/workers/executeTask';
import {statistics} from '../src/parsers/navierFile';
import {InMemoryDataSource} from '../src/data/InMemoryDataSource';
import {fieldBytes,workerCopy} from '../src/data/buffers';
import {effectiveRange} from '../src/visualization/colormap/colormaps';
import {scalarSlice} from '../src/visualization/scalar/scalarSlice';
import {defaultSettings} from '../src/visualization/settings';
import type {FieldData} from '../src/data/types';
function field():FieldData{const meta=parseMetadata({nx:2,ny:3,nz:2,fields:['arbitrary']});const values=Float64Array.from({length:12},(_,i)=>i+.123456789);return {field:meta.fields[0]!,values,dimensions:meta.dimensions,sourceDimensions:meta.dimensions,stride:1,stats:statistics(values)};}
describe('adapter and rendering boundaries',()=>{
 it('supports science-only fields and worker preparation without mutating originals',async()=>{const f=field(),source=new InMemoryDataSource({id:'test',name:'test',metadata:parseMetadata({nx:2,ny:3,nz:2,fields:['arbitrary']})},[{step:0n,time:0}],new Map([['0:arbitrary',f]]));const data=await source.getField(0n,'arbitrary');expect(data.renderValues).toBeUndefined();expect(fieldBytes(data)).toBe(96);const copy=workerCopy(data);expect(copy.values.buffer).not.toBe(data.values.buffer);const response=await executeTask({kind:'prepare',field:copy});if(Array.isArray(response.result)||!('values' in response.result))throw new Error('wrong result');expect(response.result.renderValues).toBeInstanceOf(Float32Array);expect(data.values[0]).toBe(.123456789);await source.close();await expect(source.getField(0n,'arbitrary')).rejects.toThrow('closed');});
 it('extracts XYZ planes in scientific order with matching UV basis',()=>{const f=field();const ready={...f,renderValues:Float32Array.from(f.values)};for(const axis of ['x','y','z'] as const){const result=scalarSlice(ready,{...structuredClone(defaultSettings),mode:'slice',sliceAxis:axis,slicePosition:1},[0,12]);const values=Array.from((result.textures[0]!.image as {data:Float32Array}).data);const expected:number[]=[];const [nx,ny,nz]=f.dimensions;for(let v=0;v<(axis==='z'?ny:nz);v++)for(let u=0;u<(axis==='x'?ny:nx);u++){const x=axis==='x'?nx-1:u,y=axis==='x'?u:axis==='y'?ny-1:v,z=axis==='z'?nz-1:v;expected.push(ready.renderValues[(z*ny+y)*nx+x]!);}expect(values).toEqual(expected);result.mesh.geometry.dispose();(result.mesh.material as {dispose():void}).dispose();result.textures.forEach(t=>t.dispose());}});
 it('rejects log of signed ranges and makes symmetric ranges explicit',()=>{expect(()=>effectiveRange(-1,2,{...defaultSettings.colors,logarithmic:true})).toThrow();expect(effectiveRange(-1,2,{...defaultSettings.colors,symmetric:true})).toEqual([-2,2]);});
 it('validates metadata instead of assuming a compatible layout',()=>{expect(()=>parseMetadata({nx:2,ny:2,fields:['a','a']})).toThrow('Duplicate');expect(()=>parseMetadata({nx:2,ny:2,fields:[{id:'a',scalarType:'float32'}]})).toThrow('float64');expect(()=>parseMetadata({nx:0,ny:2,fields:['a']})).toThrow('dimension');});
});
describe('partial HTTP transport',()=>{
 it('rejects full-file fallback before reading body',async()=>{const cancel=vi.fn(),arrayBuffer=vi.fn();vi.stubGlobal('fetch',vi.fn(async()=>({status:200,headers:new Headers(),body:{cancel},arrayBuffer})));try{await expect(createReader({kind:'http',url:'http://local/test',size:1000}).read(10,20)).rejects.toThrow('206');expect(cancel).toHaveBeenCalledOnce();expect(arrayBuffer).not.toHaveBeenCalled();}finally{vi.unstubAllGlobals();}});
 it('propagates in-flight cancellation to range transport',async()=>{let requested!:()=>void;const started=new Promise<void>(resolve=>requested=resolve);vi.stubGlobal('fetch',vi.fn((_url:unknown,options:{signal:AbortSignal})=>new Promise((_resolve,reject)=>{requested();options.signal.addEventListener('abort',()=>reject(new DOMException('aborted','AbortError')));})));try{const controller=new AbortController(),pending=createReader({kind:'http',url:'http://local/test',size:1000}).read(10,20,controller.signal);await started;controller.abort();await expect(pending).rejects.toMatchObject({name:'AbortError'});}finally{vi.unstubAllGlobals();}});
});
