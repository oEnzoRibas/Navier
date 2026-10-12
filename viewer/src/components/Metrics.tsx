import {useState} from 'react';
import type {NavierDataSource} from '../data/DataSource';
import type {FrameInfo} from './VisualizationViewport';
import type {MetricSeries} from '../data/types';
export function Metrics({source,info,visited}:{source:NavierDataSource;info?:FrameInfo;visited:readonly FrameInfo[]}){
  const series:readonly MetricSeries[]=source.dataset.metadata.metrics??[];
  const [metricId,setMetricId]=useState('');
  const metric=series.find(s=>s.id===metricId)??series[0];
  const samples=metric?.values.filter((_p,i)=>i%Math.max(1,Math.ceil(metric.values.length/512))===0)??[];
  const sampleMin=samples.length?Math.min(...samples.map(p=>p.value)):0,sampleMax=samples.length?Math.max(...samples.map(p=>p.value)):1;
  const start=samples[0]?.time??0,finish=samples.at(-1)?.time??1;
  const metricPoints=samples.map(p=>`${(p.time-start)/(finish-start||1)*100},${28-(p.value-sampleMin)/(sampleMax-sampleMin||1)*26}`).join(' ');
  const values=visited.map(v=>v.stats.mean),min=values.length?Math.min(...values):0,max=values.length?Math.max(...values):1;
  const points=values.map((v,i)=>`${(i/Math.max(1,values.length-1))*100},${28-(v-min)/(max-min||1)*26}`).join(' ');
  return <div className="metrics-content"><div><h3>Campo atual</h3><dl className="stat-grid"><div><dt>Mínimo</dt><dd>{info?.stats.min.toPrecision(6)??'—'}</dd></div><div><dt>Máximo</dt><dd>{info?.stats.max.toPrecision(6)??'—'}</dd></div><div><dt>Média</dt><dd>{info?.stats.mean.toPrecision(6)??'—'}</dd></div><div><dt>Amostras inválidas</dt><dd>{info?.stats.invalidCount??'—'}</dd></div></dl></div><div><h3>Média · estados visitados ({visited.length})</h3>{visited.length>1?<svg viewBox="0 0 100 30" preserveAspectRatio="none" aria-label="Média do campo nos estados carregados"><polyline points={points} fill="none" stroke="var(--accent)" strokeWidth=".6"/></svg>:<p className="muted">A série será formada conforme os estados forem carregados.</p>}<p className="muted tiny">Não há varredura automática de payloads para gerar métricas.</p></div><div><h3>Métricas do dataset</h3>{series.length?<><select aria-label="Métrica do dataset" value={metric?.id??''} onChange={e=>setMetricId(e.target.value)}>{series.map(s=><option key={s.id} value={s.id}>{s.label}{s.unit?` [${s.unit}]`:''}</option>)}</select><svg viewBox="0 0 100 30" preserveAspectRatio="none" aria-label={`Série ${metric?.label??''}`}><polyline points={metricPoints} fill="none" stroke="var(--accent)" strokeWidth=".6"/></svg><p className="muted tiny">{metric?.values.length} pontos fornecidos · eixo X: tempo</p></>:<p className="muted">Este arquivo não fornece séries indexadas de métricas.</p>}</div></div>;
}
