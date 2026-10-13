import {marchingTetrahedra} from './isosurface';
import type {FieldData,Dimensions} from '../data/types';
export interface GeometryOptions {kind:'glyphs'|'streamlines'|'cloud'|'isosurface';isoValue?:number;seeds:number;length:number;stepSize:number;scale:number;maxPoints:number;thresholdLow:number;thresholdHigh:number;absolute:boolean}
export interface ScientificGeometry {positions:Float32Array;scalars:Float32Array;normals?:Float32Array;sampleDimensions?:Dimensions;lodLevels?:number}
function interpolate(field:FieldData,x:number,y:number,z:number):number{
  const [nx,ny,nz]=field.dimensions;x=Math.max(0,Math.min(nx-1,x));y=Math.max(0,Math.min(ny-1,y));z=Math.max(0,Math.min(nz-1,z));
  const ix=Math.floor(x),iy=Math.floor(y),iz=Math.floor(z),tx=x-ix,ty=y-iy,tz=z-iz;let sum=0;
  for(let dz=0;dz<2;dz++)for(let dy=0;dy<2;dy++)for(let dx=0;dx<2;dx++)sum+=(field.values[(Math.min(iz+dz,nz-1)*ny+Math.min(iy+dy,ny-1))*nx+Math.min(ix+dx,nx-1)]??NaN)*(dx?tx:1-tx)*(dy?ty:1-ty)*(dz?tz:1-tz);
  return sum;
}
export function buildGeometry(scalar:FieldData,components:readonly FieldData[],o:GeometryOptions):ScientificGeometry {
  if(o.kind==='isosurface')return marchingTetrahedra(scalar,o.isoValue??.01);
  const dims=scalar.dimensions, positions:number[]=[],scalars:number[]=[];
  const point=(x:number,y:number,z:number)=>[dims[0]>1?x/(dims[0]-1)-.5:0,dims[1]>1?y/(dims[1]-1)-.5:0,dims[2]>1?z/(dims[2]-1)-.5:0];
  const add=(p:number[],value:number)=>{positions.push(p[0]??0,p[1]??0,p[2]??0);scalars.push(value);};
  const vector=(x:number,y:number,z:number)=>['x','y','z'].map(axis=>{const f=components.find(c=>c.field.component===axis);return f?interpolate(f,x,y,z):0;});
  const accepted=(value:number)=>Number.isFinite(value)&&(o.absolute?Math.abs(value):value)>=o.thresholdLow&&(o.absolute?Math.abs(value):value)<=o.thresholdHigh;
  if(o.kind==='cloud'){
    const total=dims[0]*dims[1]*dims[2],stride=Math.max(1,Math.ceil((total/o.maxPoints)**(1/(dims[2]>1?3:2))));
    for(let z=0;z<dims[2];z+=stride)for(let y=0;y<dims[1];y+=stride)for(let x=0;x<dims[0];x+=stride){const value=scalar.values[(z*dims[1]+y)*dims[0]+x]??NaN;if(accepted(value))add(point(x,y,z),value);}
  }else if(o.kind==='glyphs'){
    const count=Math.min(o.seeds,512);
    for(let seed=0;seed<count;seed++){
      const x=((seed*.61803398875+.13)%1)*(dims[0]-1),y=((seed*.41421356237+.31)%1)*(dims[1]-1),z=dims[2]>1?((seed*.73205080757+.41)%1)*(dims[2]-1):0;
      const value=interpolate(scalar,x,y,z);if(!accepted(value))continue;
      const v=vector(x,y,z),magnitude=Math.hypot(...v);if(!Number.isFinite(magnitude)||magnitude<1e-12)continue;
      const a=point(x,y,z),direction=v.map(c=>c/magnitude),length=Math.min(.2,o.scale*magnitude),b=a.map((c,i)=>c+(direction[i]??0)*length);
      add(a,value);add(b,value);
      const axis=Math.abs(direction[2]??0)<.8?[0,0,1]:[0,1,0];
      const cross=[(direction[1]??0)*(axis[2]??0)-(direction[2]??0)*(axis[1]??0),(direction[2]??0)*(axis[0]??0)-(direction[0]??0)*(axis[2]??0),(direction[0]??0)*(axis[1]??0)-(direction[1]??0)*(axis[0]??0)];const norm=Math.hypot(...cross)||1;
      for(const sign of [-1,1]){const c=b.map((p,i)=>p-(direction[i]??0)*length*.25+sign*(cross[i]??0)/norm*length*.14);add(b,value);add(c,value);}
    }
  }else{
    if(components.length<2)throw new Error('Streamlines require vector components');
    const count=Math.min(o.seeds,512),steps=Math.min(o.length,500),step=Math.max(.05,Math.min(o.stepSize,4));
    for(let seed=0;seed<count;seed++){
      let x=((seed*.61803398875+.13)%1)*(dims[0]-1),y=((seed*.41421356237+.31)%1)*(dims[1]-1),z=dims[2]>1?((seed*.73205080757+.41)%1)*(dims[2]-1):0;
      for(let n=0;n<steps;n++){
        const v=vector(x,y,z),mag=Math.hypot(...v);if(!Number.isFinite(mag)||mag<1e-12)break;
        const middle=[x+(v[0]??0)/mag*step*.5,y+(v[1]??0)/mag*step*.5,z+(v[2]??0)/mag*step*.5];const vm=vector(middle[0]??0,middle[1]??0,middle[2]??0),mm=Math.hypot(...vm);if(!Number.isFinite(mm)||mm<1e-12)break;
        const xx=x+(vm[0]??0)/mm*step,yy=y+(vm[1]??0)/mm*step,zz=z+(vm[2]??0)/mm*step;
        if(xx<0||yy<0||zz<0||xx>dims[0]-1||yy>dims[1]-1||zz>dims[2]-1)break;
        const value=interpolate(scalar,x,y,z);if(accepted(value)){add(point(x,y,z),value);add(point(xx,yy,zz),value);}
        x=xx;y=yy;z=zz;if(scalars.length>120_000)break;
      }
      if(scalars.length>120_000)break;
    }
  }
  return {positions:new Float32Array(positions),scalars:new Float32Array(scalars)};
}
