import type {CheckpointDescriptor} from '../data/types';
export function findStep(checkpoints:readonly CheckpointDescriptor[],step:bigint):number {
  let lo=0,hi=checkpoints.length-1;
  while(lo<=hi){const middle=Math.floor((lo+hi)/2),value=checkpoints[middle]?.step;if(value===undefined)return -1;if(value===step)return middle;if(value<step)lo=middle+1;else hi=middle-1;}
  return -1;
}
export function moveIndex(current:number,count:number,delta:number,loop=false):number {if(count===0)return 0;const next=current+delta;return loop?((next%count)+count)%count:Math.min(count-1,Math.max(0,next));}
export function parseStep(text:string):bigint {if(!/^\d+$/.test(text))throw new Error('Passo deve ser um inteiro uint64');const step=BigInt(text);if(step>18446744073709551615n)throw new Error('Passo excede uint64');return step;}
