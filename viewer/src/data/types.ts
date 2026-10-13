export type DatasetId = string;
export type Dimensions = readonly [number, number, number];
export type NumericArray = Float32Array | Float64Array;
export interface FieldRegion {origin:Dimensions;count:Dimensions;stride:Dimensions}
export interface FieldDescriptor {
  id: string; name: string; kind: 'scalar' | 'vector-component' | 'derived';
  scalarType: 'float32' | 'float64'; dimensions: Dimensions;
  unit?: string; vectorGroup?: string; component?: 'x' | 'y' | 'z';
  dependencies?: readonly string[]; operation?: 'magnitude' | 'curl';
}
export interface SimulationMetadata {
  solver?: string; dimensions: Dimensions; fields: readonly FieldDescriptor[];
  dt?: number; boundary?: string; spacing: Dimensions; parameters: Readonly<Record<string, unknown>>;
  metrics?: readonly MetricSeries[];
}
export interface SimulationDataset { id: DatasetId; name: string; metadata: SimulationMetadata; fileSize?: number }
export interface CheckpointDescriptor { step: bigint; time: number; fields?: readonly string[] }
export interface StepRange { begin: bigint; end: bigint }
export interface FieldStatistics { min: number; max: number; mean: number; finiteCount: number; invalidCount: number }
export interface FieldData {
  field: FieldDescriptor; values: NumericArray; renderValues?: Float32Array;
  dimensions: Dimensions; sourceDimensions: Dimensions; stride: number; stats: FieldStatistics;
  region?:FieldRegion;
}
export interface RenderFieldData extends FieldData { renderValues: Float32Array }
export interface MetricPoint { step: bigint; time: number; value: number }
export interface MetricSeries { id: string; label: string; unit?: string; values: readonly MetricPoint[] }
export interface ReadOptions { signal?: AbortSignal; maxSamples?: number; onProgress?: (progress: LoadProgress) => void }
export interface LoadProgress { phase: 'opening' | 'scanning' | 'loading' | 'deriving'; bytesRead: number; totalBytes?: number; completed?: number }
export interface SourceDiagnostics { bytesRead: number; rangeReads: number; cacheBytes: number; cacheLimit: number; cacheEntries: number; cacheHits: number; cacheMisses: number; lastLoadMs: number; lastDerivedMs: number }
export interface QueryResult { checkpoints: readonly CheckpointDescriptor[] }
