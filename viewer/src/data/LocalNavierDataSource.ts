import type {NavierDataSource} from './DataSource';
import type {CheckpointDescriptor,FieldData,ReadOptions,SimulationDataset,SourceDiagnostics,StepRange} from './types';
import {ByteLRU} from './cache/ByteLRU';
import {checkAbort} from './errors';
import {parseMetadata} from '../parsers/metadata';
import {planSampling} from '../parsers/navierFile';
import type {DirectoryEntry} from '../parsers/navierFile';
import type {ReaderSpec} from '../parsers/rangeReader';
import {WorkerExecutor} from '../parsers/workers/WorkerExecutor';
import type {TaskExecutor} from '../parsers/workers/WorkerExecutor';
import {derivedDescriptors} from '../analysis/derived';
import {fieldBytes,workerCopy} from './buffers';
export class LocalNavierDataSource implements NavierDataSource {
  readonly dataset:SimulationDataset;
  private directory:DirectoryEntry[]=[];private byStep=new Map<bigint,DirectoryEntry>();
  private cache=new ByteLRU<FieldData>(128*1024*1024);private shutdown=new AbortController();
  private bytesRead=0;private rangeReads=0;private hits=0;private misses=0;private loadMs=0;private derivedMs=0;
  private constructor(private spec:ReaderSpec,name:string,raw:unknown,private executor:TaskExecutor){
    const base=parseMetadata(raw),fields=[...base.fields,...derivedDescriptors(base)];
    this.dataset={id:crypto.randomUUID(),name,fileSize:spec.kind==='blob'?spec.file.size:spec.size,metadata:{...base,fields}};
  }
  static async open(file:File,metadata:unknown,options:ReadOptions={},executor:TaskExecutor=new WorkerExecutor()):Promise<LocalNavierDataSource>{return this.create({kind:'blob',file},file.name,metadata,options,executor);}
  static async openHttp(name:string,url:string,size:number,metadata:unknown,options:ReadOptions={},executor:TaskExecutor=new WorkerExecutor()):Promise<LocalNavierDataSource>{return this.create({kind:'http',url,size},name,metadata,options,executor);}
  private static async create(spec:ReaderSpec,name:string,raw:unknown,options:ReadOptions,executor:TaskExecutor):Promise<LocalNavierDataSource>{
    let source:LocalNavierDataSource|undefined;
    try{
      source=new LocalNavierDataSource(spec,name,raw,executor);
      const m=source.dataset.metadata,baseCount=m.fields.filter(f=>f.kind!=='derived').length;
      const response=await executor.run<DirectoryEntry[]>({kind:'scan',spec,expectedCount:m.dimensions[0]*m.dimensions[1]*m.dimensions[2]*baseCount},options.signal,options.onProgress);
      source.directory=response.result;source.byStep=new Map(source.directory.map(e=>[e.step,e]));source.bytesRead=response.bytesRead;source.rangeReads=response.rangeReads;
      return source;
    }catch(error){executor.close();throw error;}
  }
  async getDatasetMetadata(){return this.dataset.metadata;}
  async listCheckpoints(range?:StepRange):Promise<CheckpointDescriptor[]>{
    if(!range)return this.directory;return this.directory.filter(e=>e.step>=range.begin&&e.step<=range.end);
  }
  async getCheckpoint(step:bigint):Promise<CheckpointDescriptor>{const checkpoint=this.byStep.get(step);if(!checkpoint)throw new Error(`Checkpoint ${step} not found`);return checkpoint;}
  async getField(step:bigint,id:string,options:ReadOptions={}):Promise<FieldData>{
    checkAbort(options.signal);checkAbort(this.shutdown.signal);
    const signal=options.signal?AbortSignal.any([options.signal,this.shutdown.signal]):this.shutdown.signal;
    const descriptor=this.dataset.metadata.fields.find(f=>f.id===id);if(!descriptor)throw new Error(`Field ${id} not found`);
    const key=`${step}:${id}:${planSampling(descriptor.dimensions,options.maxSamples).stride}`;
    const cached=this.cache.get(key);if(cached){this.hits++;return cached;}this.misses++;
    let value:FieldData;
    if(descriptor.kind==='derived'){
      const inputs=await this.getFields(step,descriptor.dependencies??[],options);
      // Preserve cached scientific arrays: the worker receives owned copies,
      // and transfers the derived result back without another numeric copy.
      const copies=inputs.map(workerCopy),transfers=copies.map(f=>f.values.buffer as ArrayBuffer);
      const response=await this.executor.run<FieldData>({kind:'derive',field:descriptor,inputs:copies,spacing:this.dataset.metadata.spacing,periodic:this.dataset.metadata.boundary?.includes('periodic')??false},signal,options.onProgress,transfers);
      value=response.result;this.derivedMs=response.elapsedMs;
    }else{
      const entry=this.byStep.get(step);if(!entry)throw new Error(`Checkpoint ${step} not found`);
      const fields=this.dataset.metadata.fields.filter(f=>f.kind!=='derived'),index=fields.findIndex(f=>f.id===id);
      const response=await this.executor.run<FieldData>({kind:'read',spec:this.spec,entry,field:descriptor,index,...(options.maxSamples!==undefined?{maxSamples:options.maxSamples}:{})},signal,options.onProgress);
      value=response.result;this.bytesRead+=response.bytesRead;this.rangeReads+=response.rangeReads;this.loadMs=response.elapsedMs;
    }
    checkAbort(signal);this.cache.set(key,value,fieldBytes(value));return value;
  }
  async getFields(step:bigint,fields:readonly string[],options:ReadOptions={}):Promise<FieldData[]>{return Promise.all(fields.map(field=>this.getField(step,field,options)));}
  async getMetricSeries(id:string,range?:StepRange){const metric=this.dataset.metadata.metrics?.find(m=>m.id===id);if(!metric)return undefined;return {...metric,values:range?metric.values.filter(p=>p.step>=range.begin&&p.step<=range.end):metric.values};}
  diagnostics():SourceDiagnostics{return {bytesRead:this.bytesRead,rangeReads:this.rangeReads,cacheBytes:this.cache.bytes,cacheLimit:this.cache.budget,cacheEntries:this.cache.size,cacheHits:this.hits,cacheMisses:this.misses,lastLoadMs:this.loadMs,lastDerivedMs:this.derivedMs};}
  setCacheLimit(bytes:number):void{this.cache.setLimit(bytes);}
  async close():Promise<void>{this.shutdown.abort();this.cache.clear();this.directory=[];this.byStep.clear();this.executor.close();}
}

