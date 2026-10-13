import {useEffect,useRef,useState} from 'react';
import type {NavierDataSource} from '../data/DataSource';
import type {CheckpointDescriptor,FieldStatistics,RenderFieldData} from '../data/types';
import type {VisualizationSettings,Probe} from '../visualization/settings';
import {VisualizationEngine} from '../visualization/VisualizationEngine';
import {GeometryWorker} from '../visualization/GeometryWorker';
import {FramePipeline} from '../visualization/FramePipeline';
import {preparationKey} from '../visualization/preparationKey';
import type {ScientificGeometry,GeometryOptions} from '../analysis/geometry';
import {isAbort,errorMessage} from '../data/errors';
import {fieldBytes} from '../data/buffers';
import {planSampling} from '../parsers/navierFile';
import {effectiveRange} from '../visualization/colormap/colormaps';
export interface FrameInfo {step:bigint;time:number;stats:FieldStatistics;dimensions:readonly number[];stride:number;range:readonly [number,number];uploadMs:number;surfaceDimensions?:readonly number[];surfaceLOD?:number;regional?:boolean}
interface Prepared {field:RenderFieldData;primaryGeometry?:ScientificGeometry;glyphs?:ScientificGeometry;streamlines?:ScientificGeometry}
interface Props {source:NavierDataSource;checkpoint:CheckpointDescriptor;next?:CheckpointDescriptor;fieldId:string;settings:VisualizationSettings;onInfo?:(info:FrameInfo)=>void;onStatus?:(loading:boolean)=>void;exportToken:number;cameraToken:number;performanceMode:boolean}
export function VisualizationViewport({source,checkpoint,next,fieldId,settings,onInfo,onStatus,exportToken,cameraToken,performanceMode}:Props){
  const canvas=useRef<HTMLCanvasElement>(null),engine=useRef<VisualizationEngine|null>(null),analysis=useRef<GeometryWorker|null>(null),pipeline=useRef(new FramePipeline<Prepared>(frame=>fieldBytes(frame.field)+[frame.primaryGeometry,frame.glyphs,frame.streamlines].reduce((bytes,g)=>bytes+(g?g.positions.byteLength+g.scalars.byteLength+(g.normals?.byteLength??0):0),0)));
  const ready=useRef<{key:string;frame:Prepared;checkpoint:CheckpointDescriptor}|undefined>(undefined),busy=useRef(false);
  const [revision,setRevision]=useState(0),[status,setStatus]=useState(''),[error,setError]=useState(''),[probe,setProbe]=useState<Probe>(),[gpu,setGpu]=useState(''),[displayed,setDisplayed]=useState<CheckpointDescriptor>();
  const live=useRef({settings,onInfo,onStatus});live.current={settings,onInfo,onStatus};
  const fixedRange=useRef<{key:string;range:readonly [number,number]}|undefined>(undefined);
  const key=preparationKey(source.dataset.id,checkpoint.step,fieldId,settings);
  const nextKey=next?preparationKey(source.dataset.id,next.step,fieldId,settings):undefined;
  useEffect(()=>{if(!canvas.current)return;try{engine.current=new VisualizationEngine(canvas.current);engine.current.setProbeCallback(setProbe);analysis.current=new GeometryWorker();}catch(e){setError(errorMessage(e));}return()=>{pipeline.current.close();analysis.current?.dispose();engine.current?.dispose();engine.current=null;};},[]);
  useEffect(()=>{
    const renderer=engine.current,worker=analysis.current;if(!renderer||!worker)return;
    let active=true;busy.current=true;live.current.onStatus?.(true);setError('');setStatus('');
    const timer=setTimeout(()=>{if(active)setStatus(`Preparando ${fieldId} · passo ${checkpoint.step}`);},400);
    pipeline.current.retain(nextKey?[key,nextKey]:[key]);
    const prepare=async(step:bigint,signal:AbortSignal):Promise<Prepared>=>{
      const dimensions=source.dataset.metadata.dimensions,s=live.current.settings;
      let maxSamples=s.quality;
      if(dimensions[2]>1&&s.mode==='volume'){const stride=Math.max(1,Math.ceil(Math.max(...dimensions)/Math.min(renderer.limits.texture3d,256)));maxSamples=Math.min(maxSamples,dimensions.reduce((a,n)=>a*Math.ceil(n/stride),1));}
      const descriptor=source.dataset.metadata.fields.find(f=>f.id===fieldId);
      const regional=!!source.getRegion&&s.mode==='slice'&&!s.vectorOverlay&&!s.streamlineOverlay&&descriptor?.operation!=='curl';
      let science;
      if(regional&&source.getRegion){
        const full=source.getFieldDimensions?.(step,fieldId)??descriptor?.dimensions??dimensions,axis=['x','y','z'].indexOf(s.sliceAxis);
        const plane=[...full];plane[axis]=1;
        const plan=planSampling(plane as unknown as readonly [number,number,number],maxSamples);
        const origin=[0,0,0];origin[axis]=Math.round(s.slicePosition*(full[axis]!-1));
        const stride=[plan.stride,plan.stride,plan.stride];stride[axis]=1;
        science=await source.getRegion(step,fieldId,{origin:origin as unknown as readonly [number,number,number],count:plan.dimensions,stride:stride as unknown as readonly [number,number,number]},{signal});
      }else science=await source.getField(step,fieldId,{signal,maxSamples});
      const field=await worker.prepare(science,signal);
      if(!field.stats.finiteCount)throw new Error('No finite values in the selected field');
      const availableGroups=new Set(source.dataset.metadata.fields.map(f=>f.vectorGroup).filter(Boolean));
      const group=availableGroups.has(s.vectorGroup)?s.vectorGroup:field.field.vectorGroup??source.dataset.metadata.fields.find(f=>f.kind==='vector-component')?.vectorGroup;
      const needsVector=['glyphs','streamlines'].includes(s.mode)||s.vectorOverlay||s.streamlineOverlay;
      const components=needsVector?await source.getFields(step,source.dataset.metadata.fields.filter(f=>f.kind==='vector-component'&&f.vectorGroup===group).map(f=>f.id),{signal,maxSamples}):[];
      const options:GeometryOptions={kind:'cloud',seeds:s.seeds,length:s.integrationLength,stepSize:s.integrationStep,scale:s.glyphScale,maxPoints:60_000,thresholdLow:-Infinity,thresholdHigh:Infinity,absolute:false};
      const prepared:Prepared={field};
      if(['cloud','wavefront','sparse'].includes(s.mode)){if(s.mode==='sparse')options.thresholdLow=Number.MIN_VALUE,options.absolute=true;prepared.primaryGeometry=await worker.compute(field,[],options,signal);}
      if(s.mode==='isosurface')prepared.primaryGeometry=await worker.compute(field,[],{...options,kind:'isosurface',isoValue:s.isoValue},signal);
      if(s.mode==='glyphs')prepared.primaryGeometry=await worker.compute(field,components,{...options,kind:'glyphs'},signal);
      if(s.mode==='streamlines')prepared.primaryGeometry=await worker.compute(field,components,{...options,kind:'streamlines'},signal);
      if(s.vectorOverlay&&s.mode!=='glyphs')prepared.glyphs=await worker.compute(field,components,{...options,kind:'glyphs'},signal);
      if(s.streamlineOverlay&&s.mode!=='streamlines')prepared.streamlines=await worker.compute(field,components,{...options,kind:'streamlines'},signal);
      return prepared;
    };
    void pipeline.current.request(key,signal=>prepare(checkpoint.step,signal)).then(frame=>{
      if(!active)return;ready.current={key,frame,checkpoint};setRevision(n=>n+1);clearTimeout(timer);setStatus('');
      if(next&&nextKey)void pipeline.current.request(nextKey,signal=>prepare(next.step,signal)).catch(()=>{});
    }).catch(e=>{if(!active||isAbort(e))return;clearTimeout(timer);renderer.clearFrame();setStatus('');setError(errorMessage(e));busy.current=false;live.current.onStatus?.(false);});
    return()=>{active=false;clearTimeout(timer);};
  },[source,key,nextKey]);
  useEffect(()=>{
    const renderer=engine.current,current=ready.current;if(!renderer||!current||current.key!==key)return;
    const frame=current.frame,field=frame.field,fixedKey=`${source.dataset.id}:${fieldId}`;
    if(fixedRange.current?.key!==fixedKey)fixedRange.current={key:fixedKey,range:[field.stats.min,field.stats.max]};
    try{
      const raw=['manual','global'].includes(settings.colors.scaleMode)?[settings.colors.min,settings.colors.max] as const:settings.colors.scaleMode==='fixed'?fixedRange.current.range:[field.stats.min,field.stats.max] as const;
      const range=effectiveRange(raw[0],raw[1],settings.colors);
      if(!renderer.updateAppearance(source.dataset.id,frame,settings,range))renderer.show(source.dataset.id,{...frame,settings,range,spacing:source.dataset.metadata.spacing});
      setError('');setDisplayed(current.checkpoint);busy.current=false;live.current.onStatus?.(false);
      live.current.onInfo?.({step:current.checkpoint.step,time:current.checkpoint.time,stats:field.stats,dimensions:field.dimensions,stride:field.stride,range,uploadMs:renderer.diagnostics().uploadMs,regional:!!field.region,...(frame.primaryGeometry?.sampleDimensions?{surfaceDimensions:frame.primaryGeometry.sampleDimensions,surfaceLOD:frame.primaryGeometry.lodLevels??0}:{})});
    }catch(e){setError(errorMessage(e));busy.current=false;live.current.onStatus?.(false);}
  },[revision,key,settings,source,fieldId]);
  useEffect(()=>{if(!exportToken||!engine.current||busy.current||error||!displayed)return;const link=document.createElement('a');link.download=`navier-${fieldId.replaceAll(':','-')}-step-${displayed.step}.png`;link.href=engine.current.exportPNG();link.click();},[exportToken]);
  useEffect(()=>{if(cameraToken)engine.current?.fit();},[cameraToken]);
  useEffect(()=>{if(!performanceMode){setGpu('');return;}const timer=setInterval(()=>{const d=engine.current?.diagnostics();if(d)setGpu(`GPU: ${d.textures} texturas · ${d.geometries} geometrias · ${d.calls} draws · render ${d.renderMs.toFixed(2)} ms`);},1000);return()=>clearInterval(timer);},[performanceMode]);
  const shown=displayed??checkpoint;
  return <div className="viewport"><canvas ref={canvas} aria-label={`Campo ${fieldId} no passo ${shown.step}`} tabIndex={0}/><div className="viewport-top"><span>{fieldId}</span><span>PASSO {shown.step.toString()} · t {shown.time.toPrecision(6)}</span></div>{status&&<div className="viewport-loading" role="status">{status}</div>}{error&&<div className="viewport-error" role="alert">{error}</div>}<div className="viewport-bottom"><span>{probe?`(${probe.x}, ${probe.y}, ${probe.z}) = ${probe.value.toPrecision(7)}`:'Arraste: orbitar / mover · roda: zoom · R: restaurar · F: enquadrar'}</span><span>{gpu}</span></div></div>;
}
