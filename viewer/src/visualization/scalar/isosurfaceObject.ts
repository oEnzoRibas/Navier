import * as THREE from 'three';
import type {ScientificGeometry} from '../../analysis/geometry';
import type {VisualizationSettings} from '../settings';
import {colorAt} from '../colormap/colormaps';
export function isosurfaceObject(data:ScientificGeometry,settings:VisualizationSettings,range:readonly [number,number]):THREE.Mesh{
  const geometry=new THREE.BufferGeometry();geometry.setAttribute('position',new THREE.BufferAttribute(data.positions,3));if(data.normals)geometry.setAttribute('normal',new THREE.BufferAttribute(data.normals,3));
  const level=settings.isoValue,t=settings.colors.logarithmic?(Math.log(level)-Math.log(range[0]))/(Math.log(range[1])-Math.log(range[0])):(level-range[0])/(range[1]-range[0]),rgb=colorAt(settings.colors.palette,t);
  const material=new THREE.MeshStandardMaterial({color:new THREE.Color(rgb[0]/255,rgb[1]/255,rgb[2]/255),side:THREE.DoubleSide,roughness:.8,metalness:0,transparent:settings.colors.opacity<1,opacity:settings.colors.opacity});
  const mesh=new THREE.Mesh(geometry,material),threshold=settings.colors.absoluteThreshold?Math.abs(level):level;mesh.visible=threshold>=settings.colors.thresholdLow&&threshold<=settings.colors.thresholdHigh;return mesh;
}
