import * as THREE from 'three';
import type {RenderFieldData} from '../../data/types';
import type {VisualizationSettings} from '../settings';
import {paletteTexture} from '../scalar/paletteShader';
export function rayVolume(field:RenderFieldData,settings:VisualizationSettings,range:readonly [number,number]):{object:THREE.Mesh;textures:THREE.Texture[]}{
  const [nx,ny,nz]=field.dimensions,texture=new THREE.Data3DTexture(field.renderValues,nx,ny,nz);texture.format=THREE.RedFormat;texture.type=THREE.FloatType;texture.minFilter=THREE.LinearFilter;texture.magFilter=THREE.LinearFilter;texture.unpackAlignment=1;texture.needsUpdate=true;
  const palette=paletteTexture(settings.colors);
  const material=new THREE.RawShaderMaterial({glslVersion:THREE.GLSL3,side:THREE.BackSide,transparent:true,depthWrite:false,
    uniforms:{uField:{value:texture},uPalette:{value:palette},uCamera:{value:new THREE.Vector3()},uDirection:{value:new THREE.Vector3()},uOrthographic:{value:false},uMin:{value:range[0]},uMax:{value:range[1]},uLog:{value:settings.colors.logarithmic},uOpacity:{value:settings.colors.opacity},uLow:{value:Number.isFinite(settings.colors.thresholdLow)?settings.colors.thresholdLow:-1e30},uHigh:{value:Number.isFinite(settings.colors.thresholdHigh)?settings.colors.thresholdHigh:1e30},uAbsolute:{value:settings.colors.absoluteThreshold}},
    vertexShader:`precision highp float;in vec3 position;uniform mat4 modelViewMatrix,projectionMatrix;out vec3 vPosition;void main(){vPosition=position;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}`,
    fragmentShader:`precision highp float;precision highp sampler3D;in vec3 vPosition;out vec4 outputColor;uniform sampler3D uField;uniform sampler2D uPalette;uniform vec3 uCamera,uDirection;uniform bool uOrthographic;uniform float uMin,uMax,uOpacity,uLow,uHigh;uniform bool uLog,uAbsolute;
    void main(){vec3 direction=uOrthographic?normalize(uDirection):normalize(vPosition-uCamera),origin=uOrthographic?vPosition-direction*4.:uCamera,inv=1./direction;vec3 a=(-vec3(.5)-origin)*inv,b=(vec3(.5)-origin)*inv;vec3 nearV=min(a,b),farV=max(a,b);float start=max(max(nearV.x,nearV.y),nearV.z),finish=min(min(farV.x,farV.y),farV.z);start=max(start,0.);if(finish<=start)discard;float stepSize=(finish-start)/128.;vec4 accumulated=vec4(0.);
    for(int i=0;i<128;i++){vec3 p=origin+direction*(start+(float(i)+.5)*stepSize);float v=texture(uField,p+vec3(.5)).r,t=uAbsolute?abs(v):v;if(v!=v||abs(v)>1e37||t<uLow||t>uHigh)continue;float value=uLog?(log(max(v,1e-30))-log(uMin))/(log(uMax)-log(uMin)):(v-uMin)/(uMax-uMin);vec3 rgb=texture(uPalette,vec2(clamp(value,0.,1.),.5)).rgb;float alpha=1.-exp(-uOpacity*stepSize*4.);accumulated.rgb+=(1.-accumulated.a)*alpha*rgb;accumulated.a+=(1.-accumulated.a)*alpha;if(accumulated.a>.98)break;}if(accumulated.a<.005)discard;outputColor=vec4(accumulated.rgb/max(accumulated.a,1e-6),accumulated.a);}`});
  const object=new THREE.Mesh(new THREE.BoxGeometry(1,1,1),material);object.onBeforeRender=(_renderer,_scene,camera)=>{const uniform=material.uniforms.uCamera;if(uniform)uniform.value=object.worldToLocal(camera.getWorldPosition(new THREE.Vector3()));
    const direction=material.uniforms.uDirection;if(direction)direction.value=camera.getWorldDirection(new THREE.Vector3()).transformDirection(new THREE.Matrix4().copy(object.matrixWorld).invert());
    const orthographic=material.uniforms.uOrthographic;if(orthographic)orthographic.value=camera instanceof THREE.OrthographicCamera;};
  return {object,textures:[texture,palette]};
}
