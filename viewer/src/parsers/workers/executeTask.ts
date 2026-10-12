import type {WorkerTask,WorkerResult} from './protocol';
import type {LoadProgress} from '../../data/types';
import {createReader} from '../rangeReader';
import {scanHeaders,readField} from '../navierFile';
import {deriveField} from '../../analysis/derived';
import {buildGeometry} from '../../analysis/geometry';
import {checkAbort} from '../../data/errors';
export async function executeTask(task:WorkerTask,signal?:AbortSignal,progress?:(p:LoadProgress)=>void):Promise<{result:WorkerResult;bytesRead:number;rangeReads:number;elapsedMs:number}> {
  checkAbort(signal);const start=performance.now();let bytesRead=0,rangeReads=0,result:WorkerResult;
  if(task.kind==='scan'||task.kind==='read'){
    const reader=createReader(task.spec,bytes=>{bytesRead+=bytes;rangeReads++;});
    result=task.kind==='scan'?await scanHeaders(reader,task.expectedCount,signal,progress):await readField(reader,task.entry,task.field,task.index,task.maxSamples,signal,progress);
  }else if(task.kind==='prepare'){result={...task.field,renderValues:Float32Array.from(task.field.values)};
  }else if(task.kind==='derive'){
    progress?.({phase:'deriving',bytesRead:0});result=deriveField(task.field,task.inputs,task.spacing,task.periodic);
  }else result=buildGeometry(task.field,task.components,task.options);
  checkAbort(signal);return {result,bytesRead,rangeReads,elapsedMs:performance.now()-start};
}
