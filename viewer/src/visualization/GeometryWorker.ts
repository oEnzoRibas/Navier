import type {FieldData,RenderFieldData} from '../data/types';
import type {GeometryOptions,ScientificGeometry} from '../analysis/geometry';
import {workerCopy} from '../data/buffers';
import {WorkerExecutor} from '../parsers/workers/WorkerExecutor';
export class GeometryWorker {
  private executor=new WorkerExecutor();
  async prepare(field:FieldData,signal:AbortSignal):Promise<RenderFieldData>{
    if(field.renderValues)return field as RenderFieldData;
    const copy=workerCopy(field);
    const response=await this.executor.run<RenderFieldData>({kind:'prepare',field:copy},signal,undefined,[copy.values.buffer as ArrayBuffer]);
    return response.result;
  }
  async compute(field:FieldData,components:readonly FieldData[],options:GeometryOptions,signal:AbortSignal):Promise<ScientificGeometry>{
    const scalar=workerCopy(field),inputs=components.map(workerCopy),transfers=[scalar.values.buffer,...inputs.map(f=>f.values.buffer)] as ArrayBuffer[];
    const response=await this.executor.run<ScientificGeometry>({kind:'geometry',field:scalar,components:inputs,options},signal,undefined,transfers);return response.result;
  }
  dispose():void{this.executor.close();}
}
