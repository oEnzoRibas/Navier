import type {WorkerTask,WorkerResult} from './protocol';
import type {LoadProgress} from '../../data/types';
import {createReader} from '../rangeReader';
import {scanHeaders,readField,planSampling,statistics} from '../navierFile';
import {deriveField} from '../../analysis/derived';
import {marchingTetrahedraAsync} from '../../analysis/isosurface';
import {buildGeometry} from '../../analysis/geometry';
import {checkAbort} from '../../data/errors';
export async function executeTask(task:WorkerTask,signal?:AbortSignal,progress?:(p:LoadProgress)=>void):Promise<{result:WorkerResult;bytesRead:number;rangeReads:number;elapsedMs:number}> {
  checkAbort(signal);const start=performance.now();let bytesRead=0,rangeReads=0,result:WorkerResult;
  if(task.kind==='decode-core'){
    const width=task.field.scalarType==='float32'?4:8,dims=task.field.dimensions,cells=dims[0]*dims[1]*dims[2];
    if(!Number.isSafeInteger(cells)||cells*width!==task.buffer.byteLength||task.buffer.byteLength>64*1048576)throw new Error('Core field response has invalid size or exceeds 64 MiB transport budget');
    const plan=planSampling(dims,task.maxSamples),values=width===4?new Float32Array(plan.cells):new Float64Array(plan.cells),view=new DataView(task.buffer);let out=0;
    for(let z=0;z<dims[2];z+=plan.stride)for(let y=0;y<dims[1];y+=plan.stride)for(let x=0;x<dims[0];x+=plan.stride){const at=((z*dims[1]+y)*dims[0]+x)*width;values[out++]=width===4?view.getFloat32(at,true):view.getFloat64(at,true);}
    result={field:task.field,values,renderValues:values instanceof Float32Array?values:Float32Array.from(values),dimensions:plan.dimensions,sourceDimensions:dims,stride:plan.stride,stats:statistics(values)};
  }else if(task.kind==='scan'||task.kind==='read'){
    const reader=createReader(task.spec,bytes=>{bytesRead+=bytes;rangeReads++;});
    result=task.kind==='scan'?await scanHeaders(reader,task.expectedCount,signal,progress):await readField(reader,task.entry,task.field,task.index,task.maxSamples,signal,progress);
  }else if(task.kind==='prepare'){result={...task.field,renderValues:Float32Array.from(task.field.values)};
  }else if(task.kind==='derive'){
    progress?.({phase:'deriving',bytesRead:0});result=deriveField(task.field,task.inputs,task.spacing,task.periodic);
  }else if(task.options.kind==='isosurface')result=await marchingTetrahedraAsync(task.field,task.options.isoValue??.01,signal);
  else result=buildGeometry(task.field,task.components,task.options);
  checkAbort(signal);return {result,bytesRead,rangeReads,elapsedMs:performance.now()-start};
}
