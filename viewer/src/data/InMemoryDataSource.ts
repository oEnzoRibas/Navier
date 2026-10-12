import type {NavierDataSource} from './DataSource';
import type {CheckpointDescriptor,FieldData,ReadOptions,SimulationDataset,StepRange} from './types';
import {checkAbort} from './errors';
export class InMemoryDataSource implements NavierDataSource {
  private closed=false;
  constructor(readonly dataset:SimulationDataset,private checkpoints:readonly CheckpointDescriptor[],private fields:ReadonlyMap<string,FieldData>){}
  async getDatasetMetadata(){return this.dataset.metadata;}
  async listCheckpoints(range?:StepRange){return this.checkpoints.filter(c=>!range||(c.step>=range.begin&&c.step<=range.end));}
  async getCheckpoint(step:bigint){const checkpoint=this.checkpoints.find(c=>c.step===step);if(!checkpoint)throw new Error(`Checkpoint ${step} not found`);return checkpoint;}
  async getField(step:bigint,field:string,options:ReadOptions={}){checkAbort(options.signal);if(this.closed)throw new Error('Data source closed');const value=this.fields.get(`${step}:${field}`);if(!value)throw new Error(`Field ${field} not found`);return value;}
  async getFields(step:bigint,fields:readonly string[],options:ReadOptions={}){return Promise.all(fields.map(f=>this.getField(step,f,options)));}
  diagnostics(){return {bytesRead:0,rangeReads:0,cacheBytes:0,cacheLimit:0,cacheEntries:0,cacheHits:0,cacheMisses:0,lastLoadMs:0,lastDerivedMs:0};}
  setCacheLimit():void{}
  async close(){this.closed=true;}
}
