/// <reference lib="webworker" />
import type {WorkerEnvelope,WorkerResponse} from './protocol';
import {executeTask} from './executeTask';
const scope=self as unknown as DedicatedWorkerGlobalScope;
const controllers=new Map<number,AbortController>();
scope.onmessage=(event:MessageEvent<WorkerEnvelope|{cancel:number}>)=>{
  if('cancel' in event.data){controllers.get(event.data.cancel)?.abort();return;}
  const {id,task}=event.data;const controller=new AbortController();controllers.set(id,controller);
  void executeTask(task,controller.signal,progress=>scope.postMessage({id,progress} satisfies WorkerResponse)).then(response=>{
    const transfers:Transferable[]=[];
    if(!Array.isArray(response.result)){
      if('values' in response.result){transfers.push(response.result.values.buffer as ArrayBuffer);if(response.result.renderValues&&response.result.renderValues.buffer!==response.result.values.buffer)transfers.push(response.result.renderValues.buffer as ArrayBuffer);}
      else {transfers.push(response.result.positions.buffer as ArrayBuffer,response.result.scalars.buffer as ArrayBuffer);if(response.result.normals)transfers.push(response.result.normals.buffer as ArrayBuffer);}
    }
    scope.postMessage({id,...response} satisfies WorkerResponse,transfers);
  }).catch((error:unknown)=>scope.postMessage({id,error:error instanceof Error?error.message:String(error),name:error instanceof Error?error.name:'Error'} satisfies WorkerResponse)).finally(()=>controllers.delete(id));
};
