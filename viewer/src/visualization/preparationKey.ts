import type {VisualizationSettings} from './settings';
/** Only data/geometry-affecting settings belong in the preparation key. */
export function preparationKey(sourceId:string,step:bigint,field:string,s:VisualizationSettings):string{
  const vectors=['glyphs','streamlines'].includes(s.mode)||s.vectorOverlay||s.streamlineOverlay;
  return JSON.stringify([sourceId,step.toString(),field,s.quality,s.mode,s.vectorOverlay,s.streamlineOverlay,...(s.mode==='isosurface'?[s.isoValue]:[]),...(vectors?[s.vectorGroup,s.seeds,s.integrationLength,s.integrationStep,s.glyphScale]:[])]);
}
