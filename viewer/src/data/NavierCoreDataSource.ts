import type {NavierDataSource} from './DataSource';
import type {CheckpointDescriptor,Dimensions,FieldData,FieldDescriptor,MetricSeries,ReadOptions,SimulationDataset,SourceDiagnostics,StepRange} from './types';
import {ByteLRU} from './cache/ByteLRU';
import {fieldBytes,workerCopy} from './buffers';
import {checkAbort} from './errors';
import {WorkerExecutor,type TaskExecutor} from '../parsers/workers/WorkerExecutor';
import {derivedDescriptors} from '../analysis/derived';
interface CoreField {id:string;scalarType:'float32'|'float64';dimensions:number[];unit?:string}
interface CoreCheckpoint {step:string;time:number;generation:string;kind:string;fields:CoreField[];metrics:{name:string;value:number;unit:string}[]}
interface CoreInfo {format:string;count:number;bytes:number;firstStep:string;lastStep:string;indexPages:number;pageSize:number;recordsScanned:number;bytesRead:number;metadataJson:string}
function descriptor(field:CoreField):FieldDescriptor{
  if(!field.id||!['float32','float64'].includes(field.scalarType)||field.dimensions.length<1||field.dimensions.length>3||field.dimensions.some(n=>!Number.isSafeInteger(n)||n<1))throw new Error('Viewer requires valid rank 1–3 core fields');
  const dims=[...field.dimensions];while(dims.length<3)dims.push(1);const dimensions=dims as unknown as Dimensions,match=/^(.*)_([xyz])$/.exec(field.id);
  return {id:field.id,name:field.id,kind:match?'vector-component':'scalar',scalarType:field.scalarType,dimensions,...(field.unit?{unit:field.unit}:{}),...(match?.[1]?{vectorGroup:match[1],component:match[2] as 'x'|'y'|'z'}:{})};
}
/** Real C++ CLI transport. No parsing of storage pages/offsets in the frontend. */
export class NavierCoreDataSource implements NavierDataSource {
  readonly dataset:SimulationDataset;private directory:CoreCheckpoint[]=[];private byStep=new Map<bigint,CoreCheckpoint>();private cache=new ByteLRU<FieldData>(128*1048576);private shutdown=new AbortController();private bytes=0;private reads=0;private hits=0;private misses=0;private loadMs=0;private derivedMs=0;
  private constructor(private url:string,name:string,info:CoreInfo,first:CoreCheckpoint,private executor:TaskExecutor){
    let parameters:Record<string,unknown>={};try{const raw:unknown=JSON.parse(info.metadataJson||'{}');if(raw&&typeof raw==='object'&&!Array.isArray(raw))parameters=raw as Record<string,unknown>;}catch{throw new Error('Core dataset metadata is not valid JSON');}
    const fields=first.fields.map(descriptor),dimensions=fields[0]?.dimensions;if(!dimensions)throw new Error('Core dataset has no fields');
    const rawSpacing=parameters.spacing;
    if(rawSpacing!==undefined&&(!Array.isArray(rawSpacing)||rawSpacing.length!==3||rawSpacing.some(n=>typeof n!=='number'||!Number.isFinite(n)||n<=0)))throw new Error('Core metadata spacing must contain three positive finite values');
    const spacing:Dimensions=Array.isArray(rawSpacing)?[Number(rawSpacing[0]),Number(rawSpacing[1]),Number(rawSpacing[2])]:[1,1,1];
    const meta={dimensions,fields,spacing,parameters,...(typeof parameters.solver==='string'?{solver:parameters.solver}:{}),...(typeof parameters.boundary==='string'?{boundary:parameters.boundary}:{})};
    this.dataset={id:crypto.randomUUID(),name,fileSize:info.bytes,metadata:{...meta,fields:[...fields,...derivedDescriptors(meta)]}};
  }
  static async open(url:string,name:string,options:ReadOptions={},executor:TaskExecutor=new WorkerExecutor()):Promise<NavierCoreDataSource>{
    let source:NavierCoreDataSource|undefined;
    try{
      let metadataBytes=0,metadataReads=0;
      const json=async<T>(suffix:string):Promise<T>=>{checkAbort(options.signal);const response=await fetch(url+suffix,{signal:options.signal});if(!response.ok)throw new Error(await response.text());metadataBytes+=Number(response.headers.get('Content-Length')??0);metadataReads++;return response.json() as Promise<T>;};
      const info=await json<CoreInfo>('/info');if(info.format!=='NAVIER02'||!info.count)throw new Error('Empty/unsupported core dataset');
      const first=await json<CoreCheckpoint>(`/checkpoint/${info.firstStep}`);source=new NavierCoreDataSource(url,name,info,first,executor);
      let begin='0',batch=1000,directoryBytes=0;for(;;){let page:{checkpoints:CoreCheckpoint[];nextStep:string|null};try{page=await json(`/list/${begin}/${batch}`);}catch(e){if(batch>1&&e instanceof Error&&/budget|maxBuffer|stdout/i.test(e.message)){batch=Math.max(1,Math.floor(batch/10));continue;}throw e;}for(const c of page.checkpoints){directoryBytes+=256+c.fields.reduce((n,f)=>n+128+f.id.length*2+(f.unit?.length??0)*2+f.dimensions.length*8,0)+c.metrics.reduce((n,m)=>n+96+m.name.length*2+m.unit.length*2,0);}if(directoryBytes>64*1048576)throw new Error('Viewer metadata directory exceeds 64 MiB budget; cursor-based timeline is future work');source.directory.push(...page.checkpoints);options.onProgress?.({phase:'opening',bytesRead:metadataBytes,completed:source.directory.length});if(source.directory.length>1_000_000)throw new Error('Viewer descriptor budget exceeded (1 million)');if(page.nextStep===null)break;if(BigInt(page.nextStep)<=BigInt(begin))throw new Error('Nonadvancing core pagination');begin=page.nextStep;}
      source.bytes=metadataBytes;source.reads=metadataReads;source.byStep=new Map(source.directory.map(c=>[BigInt(c.step),c]));const metrics=new Map<string,MetricSeries>();for(const c of source.directory)for(const m of c.metrics){const series=metrics.get(m.name)??{id:m.name,label:m.name,unit:m.unit,values:[]};(series.values as {step:bigint;time:number;value:number}[]).push({step:BigInt(c.step),time:c.time,value:m.value});metrics.set(m.name,series);}source.dataset.metadata={...source.dataset.metadata,metrics:[...metrics.values()]};return source;
    }catch(e){executor.close();throw e;}
  }
  async getDatasetMetadata(){return this.dataset.metadata;}
  async listCheckpoints(range?:StepRange):Promise<CheckpointDescriptor[]>{return this.directory.filter(c=>!range||(BigInt(c.step)>=range.begin&&BigInt(c.step)<=range.end)).map(c=>({step:BigInt(c.step),time:c.time,fields:c.fields.map(f=>f.id)}));}
  async getCheckpoint(step:bigint):Promise<CheckpointDescriptor>{const c=this.byStep.get(step);if(!c)throw new Error('Checkpoint not found');return {step,time:c.time,fields:c.fields.map(f=>f.id)};}
  async getField(step:bigint,id:string,options:ReadOptions={}):Promise<FieldData>{
    checkAbort(options.signal);checkAbort(this.shutdown.signal);const signal=options.signal?AbortSignal.any([options.signal,this.shutdown.signal]):this.shutdown.signal,key=`${step}:${id}:${options.maxSamples??'full'}`;
    const cached=this.cache.get(key);if(cached){this.hits++;return cached;}this.misses++;const started=performance.now();let value:FieldData;
    const derived=this.dataset.metadata.fields.find(f=>f.id===id&&f.kind==='derived');
    if(derived){const copies=(await this.getFields(step,derived.dependencies??[],options)).map(workerCopy);const result=await this.executor.run<FieldData>({kind:'derive',field:derived,inputs:copies,spacing:this.dataset.metadata.spacing,periodic:this.dataset.metadata.boundary?.includes('periodic')??false},signal,options.onProgress,copies.map(f=>f.values.buffer as ArrayBuffer));value=result.result;this.derivedMs=result.elapsedMs;}
    else{
      const cp=this.byStep.get(step),raw=cp?.fields.find(f=>f.id===id);if(!raw)throw new Error('Core field unavailable at selected checkpoint');const field=descriptor(raw),expected=field.dimensions.reduce((a,n)=>a*n,1)*(field.scalarType==='float32'?4:8);if(!Number.isSafeInteger(expected)||expected>64*1048576)throw new Error('Core transport field exceeds 64 MiB; regional reads are future work');
      const response=await fetch(`${this.url}/field/${step}/${encodeURIComponent(id)}`,{signal});if(!response.ok)throw new Error(await response.text());if(Number(response.headers.get('Content-Length'))!==expected)throw new Error('Invalid core field response length');
      const buffer=await response.arrayBuffer();this.bytes+=buffer.byteLength;this.reads++;options.onProgress?.({phase:'loading',bytesRead:buffer.byteLength});const result=await this.executor.run<FieldData>({kind:'decode-core',buffer,field,...(options.maxSamples!==undefined?{maxSamples:options.maxSamples}:{})},signal,options.onProgress,[buffer]);value=result.result;
    }
    checkAbort(signal);this.loadMs=performance.now()-started;this.cache.set(key,value,fieldBytes(value));return value;
  }
  async getFields(step:bigint,fields:readonly string[],options:ReadOptions={}){return Promise.all(fields.map(f=>this.getField(step,f,options)));}
  async getMetricSeries(id:string,range?:StepRange){const series=this.dataset.metadata.metrics?.find(m=>m.id===id);return series?{...series,values:range?series.values.filter(v=>v.step>=range.begin&&v.step<=range.end):series.values}:undefined;}
  diagnostics():SourceDiagnostics{return {bytesRead:this.bytes,rangeReads:this.reads,cacheBytes:this.cache.bytes,cacheLimit:this.cache.budget,cacheEntries:this.cache.size,cacheHits:this.hits,cacheMisses:this.misses,lastLoadMs:this.loadMs,lastDerivedMs:this.derivedMs};}
  setCacheLimit(bytes:number){this.cache.setLimit(bytes);}
  async close(){this.shutdown.abort();this.executor.close();this.cache.clear();this.directory=[];this.byStep.clear();}
}
