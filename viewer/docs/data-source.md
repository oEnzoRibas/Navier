# Contrato NavierDataSource

`src/data/DataSource.ts` expõe dataset, getDatasetMetadata, listCheckpoints(range), getCheckpoint(step), getField, getFields, getMetricSeries opcional, diagnostics, setCacheLimit e close. Steps são bigint; JSON externo deve transportar uint64 como string. FieldDescriptor descreve id, nome, unidade opcional, dimensões, precisão, grupo/componente e dependências de derivados.

NavierCoreDataSource implementa esse contrato com a CLI C++ real via proxy local Vite. Solicita descritores paginados e o campo selecionado, sem interpretar páginas do índice no browser. NAVIER02 oferece f32/f64 e formas por checkpoint; viewer aceita rank 1–3. Métricas salvas vêm dos diretórios. O limite de timeline é aproximadamente 64 MiB; paginação totalmente por cursor e consultas regionais ainda requerem extensão. Detalhes em [integração](../../docs/viewer.md).

## Compatibilidade NAVIER01

8 bytes ASCII NAVIER01. Cada registro: uint64 step LE, float64 time LE, uint64 count LE, count valores IEEE-754 binary64 LE. Cabeçalho = 24 bytes. Campos são planos consecutivos, na ordem de fields no sidecar; X varia mais rápido, seguido por Y e Z. nx,ny e nz opcional determinam uma grade uniforme. Quantidade exata precisa corresponder às dimensões × campos. Metadados são limitados a 1 MiB na abertura pela UI.

Esse layout é suportado pelo adapter legado. NAVIER02 usa NavierCoreDataSource. Float32/interleaved/compactado exigem outro adapter. Assinatura, versão, dimensions, field ids duplicados, counts, offsets, bounds e truncamentos são verificados. Registros duplicados usam a última ocorrência; o diretório fica ordenado por step. Campos *_x, *_y, *_z identificam grupos vetoriais genericamente; magnitude depende das componentes disponíveis, curl requer xy em 2D ou xyz em 3D. Derivados usam espaçamento/stride, diferenças centrais, bordas unilaterais ou periódicas quando declaradas.

## Memória

LRU conta os bytes de todos os buffers retidos, científicos e render, sem contar duas vezes quando compartilham buffer. Chave = step/field/stride. A UI divide um orçamento global entre os datasets abertos; oversized não entra no cache. Array científico do cache nunca é transferido para um worker (seria destacado): análise transfere uma cópia própria. Workers transferem os resultados de volta. Consumidores tratam buffers como read-only. Não há deduplicação de pedidos concorrentes no momento.

Prefetch prepara o campo e a geometria do próximo checkpoint, mantendo no máximo atual/próximo com orçamento de 64 MiB por viewport; é promovido ao avançar, e cancelado somente quando fica obsoleto. Dependências vetoriais são buscadas somente quando requeridas por derivado/overlay. Cache não é limite do processo inteiro: campo corrente, comparação, inputs, cópias, buffers temporários e GPU adicionam memória. IO de tarefas abortadas pode não aparecer no contador concluído.
