import {it,expect,vi} from 'vitest';
import {NavierCoreDataSource} from '../src/data/NavierCoreDataSource';
import {InlineExecutor} from '../src/parsers/workers/WorkerExecutor';
import {scalarSlice} from '../src/visualization/scalar/scalarSlice';
import {defaultSettings} from '../src/visualization/settings';
import {preparationKey} from '../src/visualization/preparationKey';
import {statistics} from '../src/parsers/navierFile';
import type {FieldRegion,RenderFieldData} from '../src/data/types';

it('reads and caches a small region from a field larger than the full-field transport budget',async()=>{
  const cp={step:'7',time:7,generation:'1',kind:'analysis',fields:[{id:'large',scalarType:'float32',dimensions:[2048,2048,32]}],metrics:[]};
  const requests:string[]=[];
  vi.stubGlobal('fetch',vi.fn(async(input:string)=>{
    requests.push(input);
    if(input.endsWith('/info'))return Response.json({format:'NAVIER02',count:1,bytes:10*1024**3,firstStep:'7',metadataJson:'{}'});
    if(input.includes('/checkpoint/'))return Response.json(cp);
    if(input.includes('/list/'))return Response.json({checkpoints:[cp],nextStep:null});
    if(input.includes('/region/7/large/1,2,16/2,3,1/1,1,1'))return new Response(new Float32Array([1,2,3,4,5,6]).buffer,{headers:{'Content-Length':'24'}});
    throw new Error('Unexpected full field request');
  }));
  try{
    const source=await NavierCoreDataSource.open('http://core/test','test',{},new InlineExecutor());
    const region:FieldRegion={origin:[1,2,16],count:[2,3,1],stride:[1,1,1]};
    const value=await source.getRegion(7n,'large',region);
    expect(value.values).toBeInstanceOf(Float32Array);expect([...value.values]).toEqual([1,2,3,4,5,6]);
    expect(value.sourceDimensions).toEqual([2048,2048,32]);expect(value.dimensions).toEqual([2,3,1]);expect(value.region).toEqual(region);
    expect(source.getFieldDimensions(7n,'large')).toEqual([2048,2048,32]);
    expect(await source.getRegion(7n,'large',region)).toBe(value);
    await expect(source.getRegion(7n,'large',{...region,origin:[2048,0,0]})).rejects.toThrow('dimensions');
    expect(requests.filter(r=>r.includes('/region/'))).toHaveLength(1);
    expect(requests.some(r=>r.includes('/field/'))).toBe(false);
    expect(source.diagnostics().bytesRead).toBe(24);await source.close();
  }finally{vi.unstubAllGlobals();}
});

it('positions all three regional planes at the original coordinates and preserves pixel values',()=>{
  for(const axis of ['x','y','z'] as const){
    const a=['x','y','z'].indexOf(axis),count:[number,number,number]=[9,9,9],origin:[number,number,number]=[0,0,0];count[a]=1;origin[a]=6;
    const values=new Float32Array(count[0]*count[1]*count[2]);for(let i=0;i<values.length;i++)values[i]=i+.25;
    const field:RenderFieldData={field:{id:'plane',name:'plane',kind:'scalar',scalarType:'float32',dimensions:[9,9,9]},values,renderValues:values,dimensions:count,sourceDimensions:[9,9,9],stride:1,stats:statistics(values),region:{origin,count,stride:[1,1,1]}};
    const result=scalarSlice(field,{...defaultSettings,mode:'slice',sliceAxis:axis,slicePosition:.75},[0,100]);
    expect(result.axis).toBe(axis);expect(result.mesh.position.getComponent(a)).toBeCloseTo(.25);
    expect(result.planeDimensions).toEqual([9,9]);
    const texture=result.textures[0]!;expect([...((texture.image as {data:Float32Array}).data)]).toEqual([...values]);
    result.textures.forEach(t=>t.dispose());result.mesh.geometry.dispose();
    const material=result.mesh.material;if(!Array.isArray(material))material.dispose();
  }
});

it('regional cut movement invalidates data preparation but colors keep the regional cache',()=>{
  const settings={...defaultSettings,mode:'slice' as const,sliceAxis:'z' as const,slicePosition:.5};
  const key=preparationKey('a',0n,'temperature',settings);
  expect(preparationKey('a',0n,'temperature',{...settings,slicePosition:.7})).not.toBe(key);
  expect(preparationKey('a',0n,'temperature',{...settings,sliceAxis:'x'})).not.toBe(key);
  expect(preparationKey('a',0n,'temperature',{...settings,colors:{...settings.colors,opacity:.9}})).toBe(key);
});

it('computes vector magnitude from regional components and keeps masked samples invalid',async()=>{
  const fields=['x','y','z'].map(axis=>({id:`electric_${axis}`,scalarType:'float32',dimensions:[4,4,4]}));
  const cp={step:'0',time:0,generation:'1',kind:'analysis',fields,metrics:[]};
  vi.stubGlobal('fetch',vi.fn(async(input:string)=>{
    if(input.endsWith('/info'))return Response.json({format:'NAVIER02',count:1,bytes:4096,firstStep:'0',metadataJson:'{}'});
    if(input.includes('/checkpoint/'))return Response.json(cp);
    if(input.includes('/list/'))return Response.json({checkpoints:[cp],nextStep:null});
    if(!input.includes('/region/'))throw new Error('Unexpected complete-field request');
    const values=input.includes('/electric_x/')?[3,NaN]:input.includes('/electric_y/')?[4,5]:[0,0];
    return new Response(new Float32Array(values).buffer,{headers:{'Content-Length':'8'}});
  }));
  try{
    const source=await NavierCoreDataSource.open('http://core/test','test',{},new InlineExecutor());
    const region:FieldRegion={origin:[0,0,2],count:[2,1,1],stride:[1,1,1]};
    const result=await source.getRegion(0n,'magnitude:electric',region);
    expect(result.values[0]).toBe(5);expect(result.values[1]).toBeNaN();
    expect(result.stats.invalidCount).toBe(1);expect(result.region).toEqual(region);expect(source.diagnostics().bytesRead).toBe(24);
    await expect(source.getRegion(0n,'curl:electric',region)).rejects.toThrow('halo');await source.close();
  }finally{vi.unstubAllGlobals();}
});
