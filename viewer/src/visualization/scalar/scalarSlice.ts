import * as THREE from 'three';
import type {RenderFieldData} from '../../data/types';
import type {VisualizationSettings} from '../settings';
import {paletteTexture,paletteFunctions,commonUniforms} from './paletteShader';
export interface SliceResult {mesh:THREE.Mesh;textures:THREE.Texture[];planeDimensions:readonly [number,number];axis:'x'|'y'|'z';location:number}
export function scalarSlice(field:RenderFieldData,settings:VisualizationSettings,range:readonly [number,number]):SliceResult {
  const [nx,ny,nz]=field.dimensions,axis=nz===1?'z':settings.sliceAxis;
  const location=Math.round(settings.slicePosition*((axis==='x'?nx:axis==='y'?ny:nz)-1));
  const width=axis==='x'?ny:nx,height=axis==='z'?ny:nz;
  const data=new Float32Array(width*height);
  for(let y=0;y<height;y++)for(let x=0;x<width;x++){
    const ix=axis==='x'?location:x,iy=axis==='x'?x:axis==='y'?location:y,iz=axis==='z'?location:y;
    data[y*width+x]=field.renderValues[(iz*ny+iy)*nx+ix]??NaN;
  }
  const texture=new THREE.DataTexture(data,width,height,THREE.RedFormat,THREE.FloatType);texture.magFilter=THREE.NearestFilter;texture.minFilter=THREE.NearestFilter;texture.unpackAlignment=1;texture.needsUpdate=true;
  const palette=paletteTexture(settings.colors);
  const material=new THREE.ShaderMaterial({uniforms:{...commonUniforms(settings.colors,range,palette),uField:{value:texture}},side:THREE.DoubleSide,transparent:true,
    vertexShader:'varying vec2 vUv; void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}',
    fragmentShader:`uniform sampler2D uField;uniform sampler2D uPalette;uniform float uMin,uMax,uLow,uHigh,uOpacity;uniform bool uLog,uAbsolute;varying vec2 vUv;${paletteFunctions}void main(){float v=texture2D(uField,vUv).r;if(!visibleValue(v))discard;gl_FragColor=vec4(mapColor(v),uOpacity);}`});
  const mesh=new THREE.Mesh(new THREE.PlaneGeometry(1,1),material);
  if(nz>1){
    const size=axis==='x'?nx:axis==='y'?ny:nz; const p=location/Math.max(1,size-1)-.5;
    if(axis==='x'){mesh.quaternion.setFromRotationMatrix(new THREE.Matrix4().makeBasis(new THREE.Vector3(0,1,0),new THREE.Vector3(0,0,1),new THREE.Vector3(1,0,0)));mesh.position.x=p;}
    else if(axis==='y'){mesh.rotation.x=Math.PI/2;mesh.position.y=p;}
    else mesh.position.z=p;
  }else mesh.scale.x=nx/Math.max(nx,ny),mesh.scale.y=ny/Math.max(nx,ny);
  return {mesh,textures:[texture,palette],planeDimensions:[width,height],axis,location};
}
