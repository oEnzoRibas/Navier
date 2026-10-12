export type RenderMode='scalar'|'slice'|'cloud'|'volume'|'glyphs'|'streamlines'|'wavefront'|'sparse';
export interface ModeDescriptor {id:RenderMode;label:string;dimensions:'2d'|'3d'|'any';requiresVector?:boolean}
export const modes:readonly ModeDescriptor[]=[
  {id:'scalar',label:'Campo escalar 2D',dimensions:'2d'},
  {id:'slice',label:'Corte X / Y / Z',dimensions:'3d'},
  {id:'cloud',label:'Nuvem de pontos',dimensions:'3d'},
  {id:'volume',label:'Volume · ray marching',dimensions:'3d'},
  {id:'glyphs',label:'Vetores',dimensions:'any',requiresVector:true},
  {id:'streamlines',label:'Linhas de fluxo',dimensions:'any',requiresVector:true},
  {id:'wavefront',label:'Frente de onda · pontos',dimensions:'3d'},
  {id:'sparse',label:'Células ocupadas',dimensions:'3d'}];
export function availableModes(is3d:boolean,hasVector:boolean){return modes.filter(m=>(m.dimensions==='any'||m.dimensions===(is3d?'3d':'2d'))&&(!m.requiresVector||hasVector));}
