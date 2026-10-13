import type { CheckpointDescriptor, Dimensions, FieldData, FieldRegion, MetricSeries, ReadOptions, SimulationDataset, SimulationMetadata, SourceDiagnostics, StepRange } from './types';
export interface NavierDataSource {
  readonly dataset: SimulationDataset;
  getDatasetMetadata(): Promise<SimulationMetadata>;
  listCheckpoints(range?: StepRange): Promise<CheckpointDescriptor[]>;
  getCheckpoint(step: bigint): Promise<CheckpointDescriptor>;
  getField(step: bigint, field: string, options?: ReadOptions): Promise<FieldData>;
  getRegion?(step:bigint,field:string,region:FieldRegion,options?:ReadOptions):Promise<FieldData>;
  getFieldDimensions?(step:bigint,field:string):Dimensions;
  getFields(step: bigint, fields: readonly string[], options?: ReadOptions): Promise<FieldData[]>;
  getMetricSeries?(metric: string, range?: StepRange): Promise<MetricSeries | undefined>;
  diagnostics(): SourceDiagnostics;
  setCacheLimit(bytes: number): void;
  close(): Promise<void>;
}
