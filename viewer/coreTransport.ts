import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import path from 'node:path';
const execute=promisify(execFile);
let active=0;
interface Waiting {resolve:()=>void;reject:(error:Error)=>void;signal:AbortSignal;cleanup:()=>void}
const waiting:Waiting[]=[];
async function acquire(signal:AbortSignal):Promise<void>{
  signal.throwIfAborted();if(active<2){active++;return;}if(waiting.length>=16)throw new Error('Core request queue full; retry after current frames finish');
  await new Promise<void>((resolve,reject)=>{const cancel=()=>{const index=waiting.indexOf(job);if(index>=0)waiting.splice(index,1);reject(new Error('Core request cancelled'));};const job:Waiting={resolve,reject,signal,cleanup:()=>signal.removeEventListener('abort',cancel)};signal.addEventListener('abort',cancel,{once:true});waiting.push(job);});
}
function release(){active--;for(;;){const job=waiting.shift();if(!job)return;job.cleanup();if(job.signal.aborted){job.reject(new Error('Core request cancelled'));continue;}active++;job.resolve();return;}}
export async function coreCommand(project:string,args:readonly string[],binary:boolean,signal:AbortSignal):Promise<string|Buffer>{
  await acquire(signal);
  try{const executable=process.env.NAVIER_CLI??path.join(project,process.platform==='win32'?'build-msvc/Release/navier.exe':'build-release/navier');const result=await execute(executable,[...args],{encoding:binary?'buffer':'utf8',windowsHide:true,signal,maxBuffer:binary?64*1048576:16*1048576,timeout:60000});return result.stdout;}
  catch(error:unknown){if(error instanceof Error&&'stderr' in error)throw new Error(String(error.stderr).slice(0,2048)||error.message);throw error;}
  finally{release();}
}
