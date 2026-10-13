import {ByteLRU} from '../data/cache/ByteLRU';
import {abortError} from '../data/errors';
/** Two retained frames: current + next. Promotion never cancels the next frame. */
export class FramePipeline<T>{
  private jobs=new Map<string,{controller:AbortController;promise:Promise<T>}>();
  private ready:ByteLRU<T>;private cachedKeys=new Set<string>();private retained=new Set<string>();
  constructor(private bytes:(value:T)=>number=()=>0,budget=64*1024*1024){this.ready=new ByteLRU<T>(budget);}
  retain(keys:readonly string[]):void{this.retained=new Set(keys);for(const [key,job] of this.jobs)if(!this.retained.has(key)){job.controller.abort();this.jobs.delete(key);}for(const key of this.cachedKeys)if(!this.retained.has(key)){this.ready.delete(key);this.cachedKeys.delete(key);}}
  request(key:string,load:(signal:AbortSignal)=>Promise<T>):Promise<T>{
    const ready=this.ready.get(key);if(ready!==undefined)return Promise.resolve(ready);
    const job=this.jobs.get(key);if(job)return job.promise;
    const controller=new AbortController();
    const promise=load(controller.signal).then(result=>{if(controller.signal.aborted)throw abortError();if(this.retained.has(key)){this.ready.set(key,result,this.bytes(result));this.cachedKeys.add(key);}return result;}).finally(()=>{if(this.jobs.get(key)?.controller===controller)this.jobs.delete(key);});
    this.jobs.set(key,{controller,promise});return promise;
  }
  close():void{for(const job of this.jobs.values())job.controller.abort();this.jobs.clear();this.ready.clear();this.cachedKeys.clear();this.retained.clear();}
}
