import type {FieldData} from './types';
export function fieldBytes(field:FieldData):number {return field.values.byteLength+(field.values.buffer===field.renderValues?.buffer?0:(field.renderValues?.byteLength??0));}
export function workerCopy(field:FieldData):FieldData {const {renderValues:_render,...science}=field;return {...science,values:field.values.slice()};}
