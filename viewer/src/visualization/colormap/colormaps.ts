export type ColormapId='sequential'|'diverging'|'thermal'|'viridis'|'grayscale';
export type ScaleMode='auto'|'fixed'|'global'|'manual';
export interface ColorSettings {palette:ColormapId;scaleMode:ScaleMode;min:number;max:number;symmetric:boolean;logarithmic:boolean;opacity:number;thresholdLow:number;thresholdHigh:number;absoluteThreshold:boolean}
const stops:Record<ColormapId,readonly (readonly [number,number,number])[]>={
  sequential:[[14,26,47],[26,83,118],[24,159,169],[122,215,186],[229,235,173]],
  diverging:[[41,114,171],[131,178,203],[229,234,237],[211,135,108],[168,47,60]],
  thermal:[[6,10,24],[65,25,93],[146,42,111],[216,80,68],[250,163,65],[252,242,173]],
  viridis:[[68,1,84],[59,82,139],[33,145,140],[94,201,98],[253,231,37]],
  grayscale:[[14,18,24],[247,250,252]]};
export function colorAt(id:ColormapId,t:number):[number,number,number]{
  const palette=stops[id],p=Math.max(0,Math.min(1,t))*(palette.length-1),index=Math.min(palette.length-2,Math.floor(p)),a=p-index,lo=palette[index],hi=palette[index+1];
  if(!lo||!hi)throw new Error('Invalid colormap');return [0,1,2].map(i=>Math.round((lo[i]??0)*(1-a)+(hi[i]??0)*a)) as [number,number,number];
}
export function colorBytes(id:ColormapId):Uint8Array{const result=new Uint8Array(256*4);for(let i=0;i<256;i++){result.set(colorAt(id,i/255),i*4);result[i*4+3]=255;}return result;}
export function gradient(id:ColormapId):string{return `linear-gradient(90deg,${stops[id].map(c=>`rgb(${c.join(',')})`).join(',')})`;}
export function effectiveRange(min:number,max:number,settings:ColorSettings):[number,number]{
  if(!Number.isFinite(min)||!Number.isFinite(max))throw new Error('Color range must be finite');
  if(settings.symmetric){const extent=Math.max(Math.abs(min),Math.abs(max));min=-extent;max=extent;}
  if(settings.logarithmic&&(min<=0||max<=0))throw new Error('Logarithmic color scales require strictly positive values');
  if(max<min)throw new Error('Color maximum must be greater than minimum');
  if(max===min){const padding=Math.max(Math.abs(min)*1e-6,1e-12);min=settings.logarithmic?min/1.000001:min-padding;max+=padding;}
  return [min,max];
}
