import {useCallback,useEffect,useRef,useState} from 'react';
import type {NavierDataSource} from '../data/DataSource';
import {LocalNavierDataSource} from '../data/LocalNavierDataSource';
import {errorMessage,isAbort} from '../data/errors';
import {defaultSettings,type VisualizationSettings} from '../visualization/settings';
import {DatasetExplorer,type DatasetSession} from '../components/DatasetExplorer';
import {FieldExplorer} from '../components/FieldExplorer';
import {Timeline} from '../components/Timeline';
import {VisualizationViewport,type FrameInfo} from '../components/VisualizationViewport';
import {Metrics} from '../components/Metrics';
import {findStep,moveIndex,parseStep} from '../state/timeline';
import {availableModes} from '../visualization/modes';
interface CatalogItem {id:string;name:string;url:string;size:number;metadata:unknown}
function savedSettings():VisualizationSettings {
  try{const raw:unknown=JSON.parse(localStorage.getItem('navier-view-settings')??'null');if(typeof raw!=='object'||raw===null)return structuredClone(defaultSettings);const saved=raw as Partial<VisualizationSettings>;const s={...structuredClone(defaultSettings),...saved,colors:{...defaultSettings.colors,...saved.colors}};s.colors.thresholdLow=typeof s.colors.thresholdLow==='number'?s.colors.thresholdLow:-Infinity;s.colors.thresholdHigh=typeof s.colors.thresholdHigh==='number'?s.colors.thresholdHigh:Infinity;return s;}catch{return structuredClone(defaultSettings);}
}
export function App(){
  const [sessions,setSessions]=useState<DatasetSession[]>([]),[activeId,setActiveId]=useState(''),[index,setIndex]=useState(0),[fieldId,setFieldId]=useState('');
  const [settings,setSettings]=useState<VisualizationSettings>(savedSettings),[playing,setPlaying]=useState(false),[speed,setSpeed]=useState(1),[loop,setLoop]=useState(true),[loading,setLoading]=useState(false);
  const [notice,setNotice]=useState(''),[opening,setOpening]=useState(false),[catalog,setCatalog]=useState<CatalogItem[]>(),[info,setInfo]=useState<FrameInfo>(),[visited,setVisited]=useState<FrameInfo[]>([]);
  const [leftOpen,setLeftOpen]=useState(true),[rightOpen,setRightOpen]=useState(true),[metricsOpen,setMetricsOpen]=useState(true),[performanceMode,setPerformanceMode]=useState(false),[theme,setTheme]=useState('dark');
  const [exportToken,setExportToken]=useState(0),[cameraToken,setCameraToken]=useState(0),[compare,setCompare]=useState(false),[compareIndex,setCompareIndex]=useState(0),[compareInput,setCompareInput]=useState('');
  const [globalBusy,setGlobalBusy]=useState(false),[cacheMiB,setCacheMiB]=useState(128),[diagnostics,setDiagnostics]=useState('');
  const input=useRef<HTMLInputElement>(null),opener=useRef<AbortController|undefined>(undefined),globalScan=useRef<AbortController|undefined>(undefined),allSessions=useRef<DatasetSession[]>([]),paletteManual=useRef(false);
  const selected=sessions.find(s=>s.source.dataset.id===activeId),checkpoint=selected?.checkpoints[index],compareCheckpoint=selected?.checkpoints[compareIndex];
  useEffect(()=>{allSessions.current=sessions;const share=Math.floor(cacheMiB*1048576/Math.max(1,sessions.length));for(const s of sessions)s.source.setCacheLimit(share);},[sessions,cacheMiB]);
  useEffect(()=>()=>{opener.current?.abort();globalScan.current?.abort();for(const s of allSessions.current)void s.source.close();},[]);
  useEffect(()=>{localStorage.setItem('navier-view-settings',JSON.stringify(settings));},[settings]);
  const activate=(session:DatasetSession)=>{
    globalScan.current?.abort();setPlaying(false);setActiveId(session.source.dataset.id);setIndex(0);setCompareIndex(0);setCompare(false);setInfo(undefined);setVisited([]);paletteManual.current=false;
    const m=session.source.dataset.metadata,field=m.fields.find(f=>f.operation==='magnitude')??m.fields[0];
    setFieldId(field?.id??'');setSettings(s=>({...s,mode:m.dimensions[2]>1?'cloud':'scalar',vectorOverlay:false,streamlineOverlay:false,colors:{...s.colors,scaleMode:'auto',symmetric:false,logarithmic:false,thresholdLow:-Infinity,thresholdHigh:Infinity}}));
  };
  async function addSource(source:NavierDataSource){
    const checkpoints=await source.listCheckpoints();if(checkpoints.length===0){await source.close();throw new Error('O arquivo não contém checkpoints');}
    if(allSessions.current.length>=8){await source.close();throw new Error('Feche um dataset antes de abrir outro (limite de 8 workers de dados)');}
    const session={source,checkpoints};setSessions(s=>[...s,session]);activate(session);
    const hash=new URLSearchParams(location.hash.slice(1));
    const requestedMode=hash.get('mode');const allowed=availableModes(source.dataset.metadata.dimensions[2]>1,source.dataset.metadata.fields.some(f=>f.kind==='vector-component'));const mode=allowed.find(m=>m.id===requestedMode);if(mode)setSettings(s=>({...s,mode:mode.id}));
    const requestedField=hash.get('field');if(requestedField&&source.dataset.metadata.fields.some(f=>f.id===requestedField))setFieldId(requestedField);
    const requestedStep=hash.get('step');if(requestedStep){try{const found=findStep(checkpoints,parseStep(requestedStep));if(found>=0)setIndex(found);}catch{/* Ignore unrelated stale view hashes. */}}
  }
  function progress(label:string){return (p:{phase:string;bytesRead:number;completed?:number})=>setNotice(`${label} · ${p.phase==='scanning'?'Indexando cabeçalhos':'Abrindo'} · ${p.completed??0} registros · ${(p.bytesRead/1024).toFixed(1)} KiB lidos`);}
  async function openFiles(files:readonly File[]){
    opener.current?.abort();const controller=new AbortController();opener.current=controller;setOpening(true);
    try{const dataFiles=files.filter(f=>f.name.endsWith('.data'));if(!dataFiles.length)throw new Error('Selecione um .data e seu metadado .data.json');
      for(const file of dataFiles){const metadataFile=files.find(f=>f.name===file.name+'.json');if(!metadataFile)throw new Error(`Selecione também ${file.name}.json`);if(metadataFile.size>1024*1024)throw new Error('Metadados excedem 1 MiB');const raw:unknown=JSON.parse(await metadataFile.text());const source=await LocalNavierDataSource.open(file,raw,{signal:controller.signal,onProgress:progress(file.name)});await addSource(source);}
      setNotice('Dataset aberto. Os payloads permanecem no arquivo até serem solicitados.');
    }catch(error){if(!isAbort(error))setNotice(errorMessage(error));else setNotice('Abertura cancelada');}finally{setOpening(false);if(input.current)input.current.value='';}
  }
  async function showCatalog(){try{const response=await fetch('/__navier/catalog');if(!response.ok)throw new Error('Exemplos locais indisponíveis neste servidor. Use Abrir arquivos.');const items:CatalogItem[]=await response.json();setCatalog(items);}catch(error){setNotice(errorMessage(error));}}
  async function openCatalog(item:CatalogItem){
    setCatalog(undefined);opener.current?.abort();const controller=new AbortController();opener.current=controller;setOpening(true);
    try{const source=await LocalNavierDataSource.openHttp(item.name,new URL(item.url,location.href).href,item.size,item.metadata,{signal:controller.signal,onProgress:progress(item.name)});await addSource(source);setNotice('Exemplo local aberto por leituras HTTP Range.');}catch(error){if(!isAbort(error))setNotice(errorMessage(error));}finally{setOpening(false);}
  }
  const frameInfo=useCallback((frame:FrameInfo)=>{
    setInfo(frame);setVisited(previous=>{const entries=previous.filter(f=>f.step!==frame.step);entries.push(frame);entries.sort((a,b)=>a.step<b.step?-1:a.step>b.step?1:0);return entries.slice(-512);});
    if(!paletteManual.current){const palette=frame.stats.min<0&&frame.stats.max>0?'diverging':'viridis';setSettings(s=>s.colors.palette===palette?s:{...s,colors:{...s.colors,palette}});}
  },[]);
  useEffect(()=>{setInfo(undefined);setVisited([]);},[activeId,fieldId]);
  useEffect(()=>{
    if(!playing||loading||!selected||!checkpoint)return;
    const timer=setTimeout(()=>{if(index===selected.checkpoints.length-1&&!loop){setPlaying(false);return;}setIndex(i=>moveIndex(i,selected.checkpoints.length,1,loop));},200/speed);
    return()=>clearTimeout(timer);
  },[playing,loading,index,selected,speed,loop,checkpoint]);
  useEffect(()=>{
    const handler=(event:KeyboardEvent)=>{
      const target=event.target;if(target instanceof HTMLElement&&(['INPUT','SELECT','TEXTAREA','BUTTON'].includes(target.tagName)||target.isContentEditable))return;if(!selected)return;
      if(event.code==='Space'){event.preventDefault();setPlaying(p=>!p);}
      if(event.key==='ArrowLeft'||event.key==='ArrowRight'){event.preventDefault();setPlaying(false);setIndex(i=>moveIndex(i,selected.checkpoints.length,(event.key==='ArrowLeft'?-1:1)*(event.shiftKey?10:1)));}
      if(event.key.toLowerCase()==='r'||event.key.toLowerCase()==='f')setCameraToken(t=>t+1);
    };window.addEventListener('keydown',handler);return()=>window.removeEventListener('keydown',handler);
  },[selected]);
  useEffect(()=>{if(!checkpoint||!fieldId)return;const query=new URLSearchParams({step:checkpoint.step.toString(),field:fieldId,mode:settings.mode});history.replaceState(null,'','#'+query.toString());},[checkpoint,fieldId,settings.mode]);
  useEffect(()=>{const timer=setInterval(()=>{let bytes=0,hits=0,reads=0;for(const s of sessions){const d=s.source.diagnostics();bytes+=d.cacheBytes;hits+=d.cacheHits;reads+=d.bytesRead;}const active=selected?.source.diagnostics();setDiagnostics(`CACHE ${(bytes/1048576).toFixed(1)} / ${cacheMiB} MiB · ${hits} hits · IO ${(reads/1048576).toFixed(1)} MiB${performanceMode&&active?` · leitura ${active.lastLoadMs.toFixed(1)} ms · derivado ${active.lastDerivedMs.toFixed(1)} ms`:''}`);},1000);return()=>clearInterval(timer);},[sessions,selected,cacheMiB,performanceMode]);
  async function computeGlobal(){
    if(globalBusy){globalScan.current?.abort();return;}if(!selected)return;const controller=new AbortController();globalScan.current=controller;setGlobalBusy(true);setPlaying(false);
    try{let min=Infinity,max=-Infinity,completed=0;for(const c of selected.checkpoints){const data=await selected.source.getField(c.step,fieldId,{maxSamples:settings.quality,signal:controller.signal});min=Math.min(min,data.stats.min);max=Math.max(max,data.stats.max);setNotice(`Intervalo global da representação LOD · ${++completed}/${selected.checkpoints.length}`);}
      setSettings(s=>({...s,colors:{...s.colors,scaleMode:'global',min,max}}));setNotice('Intervalo global calculado. Estados foram lidos sequencialmente, sem manter todos em RAM.');
    }catch(error){setNotice(isAbort(error)?'Varredura global cancelada':errorMessage(error));}finally{setGlobalBusy(false);}
  }
  function close(id:string){const target=sessions.find(s=>s.source.dataset.id===id);if(!target)return;void target.source.close();const remaining=sessions.filter(s=>s!==target);setSessions(remaining);if(id===activeId){const first=remaining[0];if(first)activate(first);else{setActiveId('');setPlaying(false);}}}
  function compareStep(){try{if(!selected)return;const target=findStep(selected.checkpoints,parseStep(compareInput));if(target<0)throw new Error('Checkpoint de comparação não encontrado');setCompareIndex(target);}catch(error){setNotice(errorMessage(error));}}
  return <div className="application" data-theme={theme} onDragOver={e=>{if(e.dataTransfer.types.includes('Files'))e.preventDefault();}} onDrop={e=>{e.preventDefault();void openFiles([...e.dataTransfer.files]);}}><header className="app-header"><div className="brand">NAVIER <span>VIEWER</span></div><div className="toolbar"><button onClick={()=>input.current?.click()}>Abrir arquivos</button><button onClick={()=>void showCatalog()}>Exemplos locais</button>{opening&&<button onClick={()=>opener.current?.abort()}>Cancelar abertura</button>}<input ref={input} hidden type="file" multiple accept=".data,.json" onChange={e=>void openFiles([...e.target.files??[]])}/></div><div className="toolbar"><button onClick={()=>setTheme(t=>t==='dark'?'light':'dark')} aria-label="Alternar tema">{theme==='dark'?'Claro':'Escuro'}</button><button onClick={()=>setPerformanceMode(p=>!p)} aria-pressed={performanceMode}>Performance</button></div></header><div className="view-toolbar"><button onClick={()=>setLeftOpen(o=>!o)} aria-pressed={leftOpen}>Datasets</button><span className="active-name">{selected?.source.dataset.name??'Nenhum dataset aberto'}</span>{selected&&<label className="checkbox"><input type="checkbox" checked={compare} onChange={e=>setCompare(e.target.checked)}/>Comparar estados</label>}{compare&&<div className="step-jump"><input aria-label="Passo de comparação" value={compareInput} placeholder="passo B" onChange={e=>setCompareInput(e.target.value)} onKeyDown={e=>{if(e.key==='Enter')compareStep();}}/><button onClick={compareStep}>Comparar</button></div>}<button onClick={()=>setRightOpen(o=>!o)} aria-pressed={rightOpen}>Campos / render</button></div>
  <div className={`content-layout ${leftOpen?'':'hide-left'} ${rightOpen?'':'hide-right'}`}>
    {leftOpen&&<DatasetExplorer sessions={sessions} selected={activeId} onSelect={id=>{const s=sessions.find(s=>s.source.dataset.id===id);if(s)activate(s);}} onClose={close}/>}<main className="visualization-area">{selected&&checkpoint?<div className={`viewport-grid ${compare?'compare':''}`}><VisualizationViewport source={selected.source} checkpoint={checkpoint} {...(selected.checkpoints[index+1]?{next:selected.checkpoints[index+1]}:{})} fieldId={fieldId} settings={settings} onInfo={frameInfo} onStatus={setLoading} exportToken={exportToken} cameraToken={cameraToken} performanceMode={performanceMode}/>{compare&&compareCheckpoint&&<VisualizationViewport source={selected.source} checkpoint={compareCheckpoint} fieldId={fieldId} settings={settings} exportToken={0} cameraToken={cameraToken} performanceMode={performanceMode}/>}</div>:<div className="empty-view"><h1>Explorar estados de uma simulação</h1><p>Abra um arquivo <code>.data</code> e seu metadado <code>.data.json</code>.</p><p className="muted">A abertura lê cabeçalhos. Apenas os campos selecionados são carregados para visualização.</p><button className="accent" onClick={()=>input.current?.click()}>Abrir dataset local</button><button onClick={()=>void showCatalog()}>Escolher exemplo do projeto</button></div>}</main>{rightOpen&&selected&&<FieldExplorer metadata={selected.source.dataset.metadata} fieldId={fieldId} settings={settings} info={info} globalBusy={globalBusy} onField={id=>{setPlaying(false);paletteManual.current=false;setFieldId(id);}} onChange={s=>{if(s.colors.palette!==settings.colors.palette)paletteManual.current=true;setSettings(s);}} onGlobal={()=>void computeGlobal()} onExport={()=>setExportToken(t=>t+1)} onCamera={()=>setCameraToken(t=>t+1)}/>}
  </div>{selected&&checkpoint&&<Timeline checkpoints={selected.checkpoints} index={index} playing={playing} loading={loading} loop={loop} speed={speed} onIndex={setIndex} onPlaying={setPlaying} onLoop={setLoop} onSpeed={setSpeed} onError={setNotice}/>}
  <section className={`metrics-region ${metricsOpen?'':'collapsed'}`}><div className="section-bar"><button onClick={()=>setMetricsOpen(o=>!o)}>{metricsOpen?'▾':'▸'} Métricas / análise</button><label>Orçamento de cache <select aria-label="Orçamento de cache" value={cacheMiB} onChange={e=>setCacheMiB(Number(e.target.value))}><option value={64}>64 MiB</option><option value={128}>128 MiB</option><option value={256}>256 MiB</option><option value={512}>512 MiB</option></select></label></div>{metricsOpen&&selected&&<Metrics source={selected.source} info={info} visited={visited}/>}</section><footer className="app-status"><span role="status" aria-live="polite">{notice||'Arquivos locais · nenhum serviço em nuvem necessário'}</span><span>{diagnostics}</span></footer>
  {catalog&&<dialog ref={element=>{if(element&&!element.open)element.showModal();}} onCancel={()=>setCatalog(undefined)}><div className="dialog-heading"><h2>Datasets existentes no projeto</h2><button aria-label="Fechar catálogo" onClick={()=>setCatalog(undefined)}>×</button></div><p className="muted">Arquivos reais servidos localmente com HTTP Range. Nenhum payload é carregado antes da seleção.</p><div className="catalog-list">{catalog.map(item=><button key={item.id} onClick={()=>void openCatalog(item)}><span>{item.name}</span><span className="muted">{(item.size/1048576).toFixed(1)} MiB</span></button>)}</div>{catalog.length===0&&<p>Nenhum arquivo com metadados foi encontrado. Use Abrir arquivos.</p>}</dialog>}</div>;
}
