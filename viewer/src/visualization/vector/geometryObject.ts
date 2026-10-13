import * as THREE from 'three';
import {LineSegments2} from 'three/addons/lines/LineSegments2.js';
import {LineSegmentsGeometry} from 'three/addons/lines/LineSegmentsGeometry.js';
import {LineMaterial} from 'three/addons/lines/LineMaterial.js';
import type {ScientificGeometry} from '../../analysis/geometry';
import type {VisualizationSettings} from '../settings';
import {colorAt} from '../colormap/colormaps';
import {paletteTexture,commonUniforms,paletteFunctions} from '../scalar/paletteShader';
export function geometryObject(data:ScientificGeometry,settings:VisualizationSettings,range:readonly [number,number],points:boolean):{object:THREE.Object3D;textures:THREE.Texture[]} {
  if(!points){
    const positions:number[]=[],colors:number[]=[];
    for(let i=0;i<data.scalars.length;i+=2){
      const a=data.scalars[i]??NaN,b=data.scalars[i+1]??NaN;
      const visible=(v:number)=>Number.isFinite(v)&&(settings.colors.absoluteThreshold?Math.abs(v):v)>=settings.colors.thresholdLow&&(settings.colors.absoluteThreshold?Math.abs(v):v)<=settings.colors.thresholdHigh;
      if(!visible(a)||!visible(b))continue;
      for(const j of [i,i+1]){
        positions.push(data.positions[j*3]??0,data.positions[j*3+1]??0,data.positions[j*3+2]??0);
        const value=data.scalars[j]??NaN,t=settings.colors.logarithmic?(Math.log(value)-Math.log(range[0]))/(Math.log(range[1])-Math.log(range[0])):(value-range[0])/(range[1]-range[0]);
        const rgb=colorAt(settings.colors.palette,t);colors.push(rgb[0]/255,rgb[1]/255,rgb[2]/255);
      }
    }
    if(!positions.length)return {object:new THREE.Group(),textures:[]};
    const geometry=new LineSegmentsGeometry();geometry.setPositions(positions);geometry.setColors(colors);
    const material=new LineMaterial({vertexColors:true,linewidth:settings.lineWidth,transparent:settings.colors.opacity<1,opacity:settings.colors.opacity});
    const object=new LineSegments2(geometry,material);object.computeLineDistances();return {object,textures:[]};
  }
  const geometry=new THREE.BufferGeometry();geometry.setAttribute('position',new THREE.BufferAttribute(data.positions,3));geometry.setAttribute('scalar',new THREE.BufferAttribute(data.scalars,1));
  const palette=paletteTexture(settings.colors),uniforms={...commonUniforms(settings.colors,range,palette),uSize:{value:settings.pointSize*Math.min(devicePixelRatio,2)}};
  const material=new THREE.ShaderMaterial({uniforms,transparent:true,depthWrite:false,vertexShader:'attribute float scalar;varying float vScalar;uniform float uSize;void main(){vScalar=scalar;vec4 p=modelViewMatrix*vec4(position,1.);gl_Position=projectionMatrix*p;gl_PointSize=uSize;}',
    fragmentShader:`uniform sampler2D uPalette;uniform float uMin,uMax,uLow,uHigh,uOpacity;uniform bool uLog,uAbsolute;varying float vScalar;${paletteFunctions}void main(){if(!visibleValue(vScalar))discard;if(length(gl_PointCoord-vec2(.5))>.5)discard;gl_FragColor=vec4(mapColor(vScalar),uOpacity);}`});
  return {object:new THREE.Points(geometry,material),textures:[palette]};
}
