import type {WorkerTask,WorkerResponse,WorkerResult} from './protocol';
import type {LoadProgress} from '../../data/types';
import {abortError,checkAbort} from '../../data/errors';
import {executeTask} from './executeTask';
export interface TaskExecutor {
  run<T extends WorkerResult>(task:WorkerTask,signal?:AbortSignal,progress?:(p:LoadProgress)=>void,transfers?:Transferable[]):Promise<{result:T;bytesRead:number;rangeReads:number;elapsedMs:number}>;
  close():void;
}
interface Pending {resolve:(response:{result:WorkerResult;bytesRead:number;rangeReads:number;elapsedMs:number})=>void;reject:(e:Error)=>void;progress?: (p:LoadProgress)=>void;cleanup:()=>void}
export class WorkerExecutor implements TaskExecutor {
  private worker:Worker;private counter=0;private pending=new Map<number,Pending>();private closed=false;
  constructor(){
    this.worker=new Worker(new URL('./data.worker.ts',import.meta.url),{type:'module'});
    this.worker.onmessage=(event:MessageEvent<WorkerResponse>)=>{
      const response=event.data,request=this.pending.get(response.id);if(!request)return;
      if('progress' in response){request.progress?.(response.progress);return;}
      request.cleanup();this.pending.delete(response.id);
      if('error' in response){const error=new Error(response.error);error.name=response.name;request.reject(error);}else request.resolve(response);
    };
    this.worker.onerror=()=>{for(const p of this.pending.values()){p.cleanup();p.reject(new Error('Data worker failed'));}this.pending.clear();};
  }
  run<T extends WorkerResult>(task:WorkerTask,signal?:AbortSignal,progress?:(p:LoadProgress)=>void,transfers:Transferable[]=[]):Promise<{result:T;bytesRead:number;rangeReads:number;elapsedMs:number}>{
    checkAbort(signal);if(this.closed)return Promise.reject(new Error('Data source worker is closed'));
    const id=++this.counter;
    return new Promise((resolve,reject)=>{
      const cancel=()=>{this.worker.postMessage({cancel:id});this.pending.get(id)?.cleanup();this.pending.delete(id);reject(abortError());};
      const cleanup=()=>signal?.removeEventListener('abort',cancel);
      this.pending.set(id,{resolve:response=>resolve({...response,result:response.result as T}),reject,cleanup,...(progress?{progress}:{})});
      signal?.addEventListener('abort',cancel,{once:true});this.worker.postMessage({id,task},transfers);
    });
  }
  close():void{this.closed=true;for(const p of this.pending.values()){p.cleanup();p.reject(abortError());}this.pending.clear();this.worker.terminate();}
}
// Only tests inject this executor; production always uses a dedicated worker.
export class InlineExecutor implements TaskExecutor {
  private closed=false;
  async run<T extends WorkerResult>(task:WorkerTask,signal?:AbortSignal,progress?:(p:LoadProgress)=>void){if(this.closed)throw new Error('Closed executor');const result=await executeTask(task,signal,progress);return {...result,result:result.result as T};}
  close():void{this.closed=true;}
}
