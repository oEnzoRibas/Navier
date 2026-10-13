import type {FieldData} from '../data/types';
import type {ScientificGeometry} from './geometry';
const corners:readonly (readonly [number,number,number])[]=[[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,1],[1,0,1],[1,1,1],[0,1,1]];
const tetrahedra:readonly (readonly number[])[]=[[0,5,1,6],[0,1,2,6],[0,2,3,6],[0,3,7,6],[0,7,4,6],[0,4,5,6]];
type Point=readonly [number,number,number];
/** Piecewise-linear marching tetrahedra. Produces triangles, not thresholded points. */
export function marchingTetrahedra(field:FieldData,level:number,maxTriangles=100000):ScientificGeometry{
  const [nx,ny,nz]=field.dimensions;if(nx<2||ny<2||nz<2||!Number.isFinite(level))throw new Error('Isosurface requires a finite level and at least 2 samples on each axis');
  const positions:number[]=[],normals:number[]=[],scalars:number[]=[];
  const triangle=(a:Point,b:Point,c:Point,direction:Point)=>{
    const u=b.map((v,i)=>v-a[i]!),v=c.map((value,i)=>value-a[i]!);let normal:Point=[u[1]!*v[2]!-u[2]!*v[1]!,u[2]!*v[0]!-u[0]!*v[2]!,u[0]!*v[1]!-u[1]!*v[0]!];const length=Math.hypot(...normal);if(length<1e-15)return;
    if(normal.reduce((sum,n,i)=>sum+n*direction[i]!,0)<0){[b,c]=[c,b];normal=normal.map(n=>-n) as unknown as Point;}
    if(scalars.length/3>=maxTriangles)throw new Error('Isosurface triangle budget exceeded. Reduce quality or change the isovalue.');
    for(const p of [a,b,c]){positions.push(...p);normals.push(...normal.map(n=>n/length));scalars.push(level);}
  };
  for(let z=0;z<nz-1;z++)for(let y=0;y<ny-1;y++)for(let x=0;x<nx-1;x++){
    const values=corners.map(c=>field.values[((z+c[2])*ny+y+c[1])*nx+x+c[0]]??NaN);if(values.some(v=>!Number.isFinite(v)))continue;
    if(values.every(v=>v<=level)||values.every(v=>v>level))continue;
    const points=corners.map(c=>[(x+c[0])/(nx-1)-.5,(y+c[1])/(ny-1)-.5,(z+c[2])/(nz-1)-.5] as Point);
    for(const tet of tetrahedra){const inside=tet.filter(i=>values[i]!<=level),outside=tet.filter(i=>values[i]!>level);if(!inside.length||!outside.length)continue;
      const centroid=(indices:number[]):Point=>[0,1,2].map(axis=>indices.reduce((sum,i)=>sum+points[i]![axis]!,0)/indices.length) as unknown as Point;
      const inner=centroid(inside),outer=centroid(outside),direction=outer.map((v,i)=>v-inner[i]!) as unknown as Point;
      const cut=(i:number,j:number):Point=>{const t=(level-values[i]!)/(values[j]!-values[i]!);return points[i]!.map((v,axis)=>v+t*(points[j]![axis]!-v)) as unknown as Point;};
      if(inside.length===1){const i=inside[0]!;triangle(cut(i,outside[0]!),cut(i,outside[1]!),cut(i,outside[2]!),direction);}
      else if(outside.length===1){const j=outside[0]!;triangle(cut(inside[0]!,j),cut(inside[1]!,j),cut(inside[2]!,j),direction);}
      else{const a=cut(inside[0]!,outside[0]!),b=cut(inside[0]!,outside[1]!),c=cut(inside[1]!,outside[0]!),d=cut(inside[1]!,outside[1]!);triangle(a,b,c,direction);triangle(b,d,c,direction);}
    }
  }
  return {positions:new Float32Array(positions),normals:new Float32Array(normals),scalars:new Float32Array(scalars)};
}
import {checkAbort} from '../data/errors';
/** Layered CPU extraction yields to cancellation; fields are already resident, not storage chunks. */
export async function marchingTetrahedraAsync(field:FieldData,level:number,signal?:AbortSignal,maxTriangles=100000):Promise<ScientificGeometry>{
  const [nx,ny,nz]=field.dimensions;if(nz<2)throw new Error('Isosurface requires 3D samples');
  const layers:ScientificGeometry[]=[];let vertices=0;
  for(let z=0;z<nz-1;z++){
    checkAbort(signal);const layer=marchingTetrahedra({...field,dimensions:[nx,ny,2],values:field.values.subarray(z*nx*ny,(z+2)*nx*ny)},level,maxTriangles-vertices/3);
    const normal=layer.normals!;
    for(let i=0;i<layer.positions.length;i+=3){layer.positions[i+2]=(layer.positions[i+2]!+.5+z)/(nz-1)-.5;normal[i+2]=normal[i+2]!*(nz-1);const length=Math.hypot(normal[i]!,normal[i+1]!,normal[i+2]!)||1;normal[i]=normal[i]!/length;normal[i+1]=normal[i+1]!/length;normal[i+2]=normal[i+2]!/length;}
    vertices+=layer.scalars.length;layers.push(layer);await new Promise<void>(resolve=>setTimeout(resolve,0));
  }
  checkAbort(signal);const positions=new Float32Array(vertices*3),normals=new Float32Array(vertices*3),scalars=new Float32Array(vertices);let offset=0;
  for(const layer of layers){positions.set(layer.positions,offset*3);normals.set(layer.normals!,offset*3);scalars.set(layer.scalars,offset);offset+=layer.scalars.length;}
  return {positions,normals,scalars};
}
