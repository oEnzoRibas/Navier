import type {Dimensions,FieldData,FieldDescriptor,LoadProgress} from '../../data/types';
import type {DirectoryEntry} from '../navierFile';
import type {ReaderSpec} from '../rangeReader';
import type {GeometryOptions,ScientificGeometry} from '../../analysis/geometry';
export type WorkerTask =
 | {kind:'scan';spec:ReaderSpec;expectedCount:number}
 | {kind:'read';spec:ReaderSpec;entry:DirectoryEntry;field:FieldDescriptor;index:number;maxSamples?:number}
 | {kind:'derive';field:FieldDescriptor;inputs:readonly FieldData[];spacing:Dimensions;periodic:boolean}
 | {kind:'prepare';field:FieldData}
 | {kind:'geometry';field:FieldData;components:readonly FieldData[];options:GeometryOptions};
export type WorkerResult = DirectoryEntry[] | FieldData | ScientificGeometry;
export interface WorkerEnvelope {id:number;task:WorkerTask}
export type WorkerResponse = {id:number;result:WorkerResult;bytesRead:number;rangeReads:number;elapsedMs:number} | {id:number;progress:LoadProgress} | {id:number;error:string;name:string};
