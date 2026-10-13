import {it,expect} from 'vitest';
import {buildGeometry,type GeometryOptions} from '../src/analysis/geometry';
import {geometryObject} from '../src/visualization/vector/geometryObject';
import {defaultSettings} from '../src/visualization/settings';
import {statistics} from '../src/parsers/navierFile';
import type {FieldData} from '../src/data/types';

const options:GeometryOptions={kind:'streamlines',seeds:64,length:100,stepSize:.6,scale:1,maxPoints:60000,thresholdLow:-Infinity,thresholdHigh:Infinity,absolute:false};
function field(id:string,values:Float64Array,component?:'x'|'y'|'z'):FieldData{
  return {field:{id,name:id,kind:component?'vector-component':'scalar',scalarType:'float64',dimensions:[9,9,9],...(component?{component,vectorGroup:'electric'}:{})},values,dimensions:[9,9,9],sourceDimensions:[9,9,9],stride:1,stats:statistics(values)};
}
it('stops integration before a masked source contaminates segment positions',()=>{
  const components=['x','y','z'].map((axis,index)=>{
    const values=new Float64Array(9**3);
    for(let z=0;z<9;z++)for(let y=0;y<9;y++)for(let x=0;x<9;x++){
      const p=[x-4,y-4,z-4];values[(z*9+y)*9+x]=Math.hypot(...p)<1.5?NaN:-p[index]!;
    }
    return field(axis,values,axis as 'x'|'y'|'z');
  });
  const geometry=buildGeometry(field('magnitude',new Float64Array(9**3).fill(1)),components,options);
  expect(geometry.positions.length).toBeGreaterThan(0);
  expect([...geometry.positions].every(Number.isFinite)).toBe(true);
});
it('zero fields produce an empty object without NaN line bounds',()=>{
  const components=['x','y','z'].map(axis=>field(axis,new Float64Array(9**3),axis as 'x'|'y'|'z'));
  const geometry=buildGeometry(field('zero',new Float64Array(9**3)),components,options);
  expect(geometry.positions.length).toBe(0);
  const result=geometryObject(geometry,defaultSettings,[0,1],false);
  expect(result.object.type).toBe('Group');
  expect(result.object.children).toHaveLength(0);
});
