import * as THREE from 'three';
import {colorBytes} from '../colormap/colormaps';
import type {ColorSettings} from '../colormap/colormaps';
export function paletteTexture(settings:ColorSettings):THREE.DataTexture {
  const texture=new THREE.DataTexture(colorBytes(settings.palette),256,1,THREE.RGBAFormat);texture.colorSpace=THREE.NoColorSpace;texture.magFilter=THREE.LinearFilter;texture.minFilter=THREE.LinearFilter;texture.needsUpdate=true;return texture;
}
export const paletteFunctions=`
  float normValue(float v) {
    if(uLog) return (log(max(v,1e-30))-log(uMin))/(log(uMax)-log(uMin));
    return (v-uMin)/(uMax-uMin);
  }
  bool visibleValue(float v) {float t=uAbsolute?abs(v):v;return v==v&&abs(v)<1e37&&t>=uLow&&t<=uHigh;}
  vec3 mapColor(float v) {return texture2D(uPalette,vec2(clamp(normValue(v),0.,1.),.5)).rgb;}
`;
export function commonUniforms(settings:ColorSettings,range:readonly [number,number],palette:THREE.DataTexture){
  return {uPalette:{value:palette},uMin:{value:range[0]},uMax:{value:range[1]},uLog:{value:settings.logarithmic},uLow:{value:Number.isFinite(settings.thresholdLow)?settings.thresholdLow:-1e30},uHigh:{value:Number.isFinite(settings.thresholdHigh)?settings.thresholdHigh:1e30},uAbsolute:{value:settings.absoluteThreshold},uOpacity:{value:settings.opacity}};
}
