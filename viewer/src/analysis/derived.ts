import type {Dimensions, FieldData, FieldDescriptor, SimulationMetadata} from '../data/types';
import {statistics} from '../parsers/navierFile';
export function derivedDescriptors(metadata:SimulationMetadata):FieldDescriptor[] {
  const groups=new Map<string,FieldDescriptor[]>();
  for(const field of metadata.fields){if(!field.vectorGroup||!field.component)continue;const fields=groups.get(field.vectorGroup)??[];fields.push(field);groups.set(field.vectorGroup,fields);}
  const result:FieldDescriptor[]=[];
  for(const [group,fields] of groups){
    const ordered=['x','y','z'].map(axis=>fields.find(f=>f.component===axis)).filter((f):f is FieldDescriptor=>!!f);
    if(ordered.length<2)continue;
    result.push({id:`magnitude:${group}`,name:`Módulo · ${group}`,kind:'derived',scalarType:'float64',dimensions:metadata.dimensions,vectorGroup:group,dependencies:ordered.map(f=>f.id),operation:'magnitude',...(ordered[0]?.unit?{unit:ordered[0].unit}:{})});
    if(fields.some(f=>f.component==='x')&&fields.some(f=>f.component==='y')&&(metadata.dimensions[2]===1||fields.some(f=>f.component==='z')))
      result.push({id:`curl:${group}`,name:`Rotacional · ${group}`,kind:'derived',scalarType:'float64',dimensions:metadata.dimensions,vectorGroup:group,dependencies:ordered.map(f=>f.id),operation:'curl',unit:'por unidade da malha'});
  }
  return result;
}
export function deriveField(field:FieldDescriptor,inputs:readonly FieldData[],spacing:Dimensions,periodic:boolean):FieldData {
  const first=inputs[0];if(!first)throw new Error('Derived field has no dependencies');
  const dims=first.dimensions, count=dims[0]*dims[1]*dims[2];
  if(inputs.some(f=>f.dimensions.some((n,i)=>n!==dims[i])))throw new Error('Derived dependency dimensions do not match');
  const values=new Float64Array(count);
  if(field.operation==='magnitude'){
    for(let i=0;i<count;i++){let sum=0;for(const input of inputs)sum+=(input.values[i]??NaN)**2;values[i]=Math.sqrt(sum);}
  }else if(field.operation==='curl'){
    const components=['x','y','z'].map(axis=>inputs.find(f=>f.field.component===axis)?.values);
    if(!components[0]||!components[1]||(first.sourceDimensions[2]>1&&!components[2]))throw new Error('Rotacional requires complete vector components');
    const derivative=(component:number,x:number,y:number,z:number,axis:number):number=>{
      const array=components[component];if(!array)return 0;
      const size=dims[axis]??1;if(size===1)return 0;
      const p=[x,y,z];const c=p[axis]??0;
      const lo=periodic?(c-1+size)%size:Math.max(0,c-1),hi=periodic?(c+1)%size:Math.min(size-1,c+1);
      const a=[...p],b=[...p];a[axis]=lo;b[axis]=hi;
      const index=(v:number[])=>((v[2]??0)*dims[1]+(v[1]??0))*dims[0]+(v[0]??0);
      const distance=(periodic?2:hi-lo)*(spacing[axis]??1)*first.stride;
      return ((array[index(b)]??NaN)-(array[index(a)]??NaN))/distance;
    };
    for(let z=0;z<dims[2];z++)for(let y=0;y<dims[1];y++)for(let x=0;x<dims[0];x++){
      const cz=derivative(1,x,y,z,0)-derivative(0,x,y,z,1);
      values[(z*dims[1]+y)*dims[0]+x]=first.sourceDimensions[2]===1?cz:Math.hypot(derivative(2,x,y,z,1)-derivative(1,x,y,z,2),derivative(0,x,y,z,2)-derivative(2,x,y,z,0),cz);
    }
  }else throw new Error('Unsupported derived operation');
  return {field,values,renderValues:Float32Array.from(values),dimensions:dims,sourceDimensions:first.sourceDimensions,stride:first.stride,stats:statistics(values)};
}
