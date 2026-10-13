import type {RenderMode} from './modes';
import type {ColorSettings} from './colormap/colormaps';
export interface VisualizationSettings {
  isoValue:number;
  mode:RenderMode;colors:ColorSettings;quality:number;sliceAxis:'x'|'y'|'z';slicePosition:number;
  seeds:number;integrationLength:number;integrationStep:number;lineWidth:number;glyphScale:number;pointSize:number;
  vectorOverlay:boolean;streamlineOverlay:boolean;projection:'perspective'|'orthographic';
}
export const defaultSettings:VisualizationSettings={isoValue:.01,mode:'scalar',colors:{palette:'viridis',scaleMode:'auto',min:0,max:1,symmetric:false,logarithmic:false,opacity:.6,thresholdLow:-Infinity,thresholdHigh:Infinity,absoluteThreshold:false},quality:2_000_000,sliceAxis:'z',slicePosition:.5,seeds:64,integrationLength:100,integrationStep:.6,lineWidth:1,glyphScale:.9,pointSize:2,vectorOverlay:false,streamlineOverlay:false,projection:'perspective'};
export interface Probe {x:number;y:number;z:number;value:number;field:string}
export interface CameraSnapshot {position:readonly number[];target:readonly number[];projection:'perspective'|'orthographic'}
