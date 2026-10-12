import {useEffect,useRef,useState} from 'react';
import type {NavierDataSource} from '../data/DataSource';
import type {CheckpointDescriptor,FieldStatistics} from '../data/types';
import type {VisualizationSettings,Probe} from '../visualization/settings';
import {VisualizationEngine} from '../visualization/VisualizationEngine';
import {GeometryWorker} from '../visualization/GeometryWorker';
import type {ScientificGeometry,GeometryOptions} from '../analysis/geometry';
import {isAbort,errorMessage} from '../data/errors';
import {effectiveRange} from '../visualization/colormap/colormaps';
export interface FrameInfo {step:bigint;time:number;stats:FieldStatistics;dimensions:readonly number[];stride:number;range:readonly [number,number];uploadMs:number}
interface Props {source:NavierDataSource;checkpoint:CheckpointDescriptor;next?:CheckpointDescriptor;fieldId:string;settings:VisualizationSettings;onInfo?:(info:FrameInfo)=>void;onStatus?:(loading:boolean)=>void;exportToken:number;cameraToken:number;performanceMode:boolean}
export function VisualizationViewport({source,checkpoint,next,fieldId,settings,onInfo,onStatus,exportToken,cameraToken,performanceMode}:Props){
  const canvas=useRef<HTMLCanvasElement>(null),engine=useRef<VisualizationEngine|null>(null),analysis=useRef<GeometryWorker|null>(null);
  const [status,setStatus]=useState('Inicializando viewport'),[error,setError]=useState(''),[probe,setProbe]=useState<Probe>(),[gpu,setGpu]=useState('');
  const callbacks=useRef({onInfo,onStatus});callbacks.current={onInfo,onStatus};
  const fixedRange=useRef<{key:string;range:readonly [number,number]}|undefined>(undefined);
  useEffect(()=>{if(!canvas.current)return;try{engine.current=new VisualizationEngine(canvas.current);engine.current.setProbeCallback(setProbe);analysis.current=new GeometryWorker();setStatus('');}catch(e){setError(errorMessage(e));}return()=>{analysis.current?.dispose();engine.current?.dispose();engine.current=null;};},[]);
  useEffect(()=>{
    const renderer=engine.current,worker=analysis.current;if(!renderer||!worker)return;
    const controller=new AbortController();callbacks.current.onStatus?.(true);setError('');setStatus(`Carregando ${fieldId} · passo ${checkpoint.step}`);
    void (async()=>{
      const dimensions=source.dataset.metadata.dimensions,is3d=dimensions[2]>1;
      let maxSamples=settings.quality;
      if(is3d&&settings.mode==='volume'){
        const stride=Math.max(1,Math.ceil(Math.max(...dimensions)/Math.min(renderer.limits.texture3d,256)));
        maxSamples=Math.min(maxSamples,dimensions.reduce((a,n)=>a*Math.ceil(n/stride),1));
      }
      const scientific=await source.getField(checkpoint.step,fieldId,{signal:controller.signal,maxSamples,onProgress:p=>setStatus(`${p.phase==='deriving'?'Calculando campo derivado':'Lendo campo'} · ${(p.bytesRead/1048576).toFixed(1)} MiB`)});
      const field=await worker.prepare(scientific,controller.signal);
      if(!field.stats.finiteCount)throw new Error('No finite values in the selected field');
      const fixedKey=`${source.dataset.id}:${fieldId}`;
      if(fixedRange.current?.key!==fixedKey)fixedRange.current={key:fixedKey,range:[field.stats.min,field.stats.max]};
      const raw=settings.colors.scaleMode==='manual'||settings.colors.scaleMode==='global'?[settings.colors.min,settings.colors.max] as const:settings.colors.scaleMode==='fixed'?fixedRange.current.range:[field.stats.min,field.stats.max] as const;
      const range=effectiveRange(raw[0],raw[1],settings.colors);
      let primaryGeometry:ScientificGeometry|undefined,glyphs:ScientificGeometry|undefined,streamlines:ScientificGeometry|undefined;
      const group=field.field.vectorGroup??source.dataset.metadata.fields.find(f=>f.kind==='vector-component')?.vectorGroup;
      const vectorFields=source.dataset.metadata.fields.filter(f=>f.kind==='vector-component'&&f.vectorGroup===group);
      const needsVector=['glyphs','streamlines'].includes(settings.mode)||settings.vectorOverlay||settings.streamlineOverlay;
      const components=needsVector?await source.getFields(checkpoint.step,vectorFields.map(f=>f.id),{signal:controller.signal,maxSamples}):[];
      const options:GeometryOptions={kind:'cloud',seeds:settings.seeds,length:settings.integrationLength,stepSize:settings.integrationStep,scale:settings.glyphScale,maxPoints:60_000,thresholdLow:-Infinity,thresholdHigh:Infinity,absolute:settings.colors.absoluteThreshold};
      if(['cloud','wavefront','sparse'].includes(settings.mode)){
        if(settings.mode==='sparse')options.thresholdLow=Number.MIN_VALUE,options.absolute=true;
        primaryGeometry=await worker.compute(field,[],options,controller.signal);
      }
      if(settings.mode==='glyphs')primaryGeometry=await worker.compute(field,components,{...options,kind:'glyphs'},controller.signal);
      if(settings.mode==='streamlines')primaryGeometry=await worker.compute(field,components,{...options,kind:'streamlines'},controller.signal);
      if(settings.vectorOverlay&&settings.mode!=='glyphs')glyphs=await worker.compute(field,components,{...options,kind:'glyphs'},controller.signal);
      if(settings.streamlineOverlay&&settings.mode!=='streamlines')streamlines=await worker.compute(field,components,{...options,kind:'streamlines'},controller.signal);
      if(controller.signal.aborted)return;
      setStatus('Enviando representação float32 para GPU');
      renderer.show(source.dataset.id,{field,settings,range,spacing:source.dataset.metadata.spacing,...(primaryGeometry?{primaryGeometry}:{}),...(glyphs?{glyphs}:{}),...(streamlines?{streamlines}:{})});
      callbacks.current.onInfo?.({step:checkpoint.step,time:checkpoint.time,stats:field.stats,dimensions:field.dimensions,stride:field.stride,range,uploadMs:renderer.diagnostics().uploadMs});
      setStatus('');callbacks.current.onStatus?.(false);
      if(next)void source.getField(next.step,fieldId,{maxSamples,signal:controller.signal}).catch(()=>{});
    })().catch(e=>{if(isAbort(e))return;renderer.clearFrame();setStatus('');setError(errorMessage(e));callbacks.current.onStatus?.(false);});
    return()=>controller.abort();
  },[source,checkpoint.step,fieldId,settings,next?.step]);
  useEffect(()=>{if(!exportToken||!engine.current||status||error)return;const link=document.createElement('a');link.download=`navier-${fieldId.replaceAll(':','-')}-step-${checkpoint.step}.png`;link.href=engine.current.exportPNG();link.click();},[exportToken]);
  useEffect(()=>{if(cameraToken)engine.current?.fit();},[cameraToken]);
  useEffect(()=>{if(!performanceMode){setGpu('');return;}const timer=setInterval(()=>{const d=engine.current?.diagnostics();if(d)setGpu(`GPU: ${d.textures} texturas · ${d.geometries} geometrias · ${d.calls} draws · render ${d.renderMs.toFixed(2)} ms`);},1000);return()=>clearInterval(timer);},[performanceMode]);
  return <div className="viewport"><canvas ref={canvas} aria-label={`Campo ${fieldId} no passo ${checkpoint.step}`} tabIndex={0}/><div className="viewport-top"><span>{fieldId}</span><span>PASSO {checkpoint.step.toString()} · t {checkpoint.time.toPrecision(6)}</span></div>{status&&<div className="viewport-message" role="status">{status}</div>}{error&&<div className="viewport-error" role="alert">{error}</div>}<div className="viewport-bottom"><span>{probe?`(${probe.x}, ${probe.y}, ${probe.z}) = ${probe.value.toPrecision(7)}`:'Arraste: orbitar / mover · roda: zoom · R: restaurar · F: enquadrar'}</span><span>{gpu}</span></div></div>;
}
