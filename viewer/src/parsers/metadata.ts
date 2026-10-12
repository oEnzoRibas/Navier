import type { Dimensions, FieldDescriptor, SimulationMetadata, MetricSeries } from '../data/types';
function record(value: unknown): Record<string, unknown> {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) throw new Error('Metadata must be a JSON object');
  return value as Record<string, unknown>;
}
function dimension(value: unknown, name: string): number {
  if (typeof value !== 'number' || !Number.isInteger(value) || value < 1 || value > 1_048_576) throw new Error(`Invalid grid dimension ${name}`);
  return value;
}
export function parseMetadata(raw: unknown): SimulationMetadata {
  const m = record(raw);
  if(m.layout!==undefined&&!['field-major','planar','planar-row-major, index=y*nx+x','planar-row-major, index=(z*ny+y)*nx+x','planar, index=(z*ny+y)*nx+x'].includes(String(m.layout)))throw new Error('Unsupported NAVIER01 layout; expected field-major planes');
  const dimensions: Dimensions = [dimension(m.nx, 'nx'), dimension(m.ny, 'ny'), dimension(m.nz ?? 1, 'nz')];
  const cells = dimensions[0] * dimensions[1] * dimensions[2];
  if (!Number.isSafeInteger(cells)) throw new Error('Grid cell count exceeds safe addressability');
  if (!Array.isArray(m.fields) || m.fields.length === 0 || m.fields.length > 256) throw new Error('Metadata requires an ordered field directory');
  const fields: FieldDescriptor[] = m.fields.map((item: unknown) => {
    const desc = typeof item === 'string' ? {id: item} : record(item);
    if (typeof desc.id !== 'string' || !desc.id) throw new Error('Invalid field identifier');
    if (desc.scalarType !== undefined && desc.scalarType !== 'float64') throw new Error('NAVIER01 payloads are float64; another scalar format needs its own adapter');
    const match = /^(.*)_([xyz])$/.exec(desc.id);
    const component = (['x','y','z'].includes(String(desc.component))?desc.component:match?.[2]) as 'x' | 'y' | 'z' | undefined;
    return {id: desc.id, name: typeof desc.name === 'string' ? desc.name : desc.id,
      kind: component ? 'vector-component' : 'scalar', scalarType: 'float64', dimensions,
      ...(typeof desc.unit === 'string' ? {unit: desc.unit} : {}),
      ...(typeof desc.vectorGroup === 'string' ? {vectorGroup: desc.vectorGroup} : match?.[1] ? {vectorGroup: match[1]} : {}),
      ...(component ? {component} : {})};
  });
  if (new Set(fields.map(f => f.id)).size !== fields.length) throw new Error('Duplicate field names in metadata');
  const spacing: Dimensions = [m.dx, m.dy, m.dz].map(v => typeof v === 'number' && Number.isFinite(v) && v > 0 ? v : 1) as unknown as Dimensions;
  if (m.dt !== undefined && (typeof m.dt !== 'number' || !Number.isFinite(m.dt))) throw new Error('Invalid metadata timestep');
  const parameters = Object.fromEntries(Object.entries(m).filter(([key]) => !['fields','nx','ny','nz','metrics'].includes(key)));
  let metrics: MetricSeries[] | undefined;
  if (Array.isArray(m.metrics)) metrics = m.metrics.map((rawSeries: unknown) => {
    const series = record(rawSeries);
    if (typeof series.id !== 'string' || !Array.isArray(series.values)) throw new Error('Invalid metric series metadata');
    return {id: series.id, label: typeof series.label === 'string' ? series.label : series.id,
      ...(typeof series.unit === 'string' ? {unit: series.unit} : {}),
      values: series.values.map((rawPoint: unknown) => {const p = record(rawPoint); const step = BigInt(String(p.step));
        if (step < 0n || step > 18446744073709551615n || typeof p.time !== 'number' || !Number.isFinite(p.time) || typeof p.value !== 'number' || !Number.isFinite(p.value)) throw new Error('Invalid metric point');
        return {step, time: p.time, value: p.value};})};
  });
  return {dimensions, fields, spacing, parameters,
    ...(typeof m.solver === 'string' ? {solver: m.solver} : {}),
    ...(typeof m.boundary === 'string' ? {boundary: m.boundary} : {}),
    ...(typeof m.dt === 'number' ? {dt: m.dt} : {}), ...(metrics ? {metrics} : {})};
}

