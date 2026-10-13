# Navier Viewer — implementação e verificação

Em 12/10/2026, o viewer foi migrado para React, TypeScript estrito, Vite, Three.js/WebGL2 e Web Workers. Mantém compatibilidade com arquivos NAVIER01 e preserva o exporter HTML C++ em legacy.

## Correção da reprodução

A leitura/preparação dependia do objeto settings inteiro: mudar palette/opacity/escala reiniciava leitura, worker e reconstrução de GPU. Prefetch compartilhava o AbortController do frame anterior, cancelado ao avançar. Isso gerava trabalho repetido e o aviso frequente sobre a viewport.

Agora preparationKey inclui apenas fonte/step/field/LOD/modo/parâmetros de geometria. FramePipeline deduplica preparações e conserva atual/próximo em cache de 64 MiB por viewport. O prefetch inclui geometria e é promovido quando o próximo frame vira corrente. Saltos cancelam tarefas obsoletas. Cores/escala/thresholds/opacidade/projeção não disparam IO ou worker. Atualizações escalar/points/volume reaproveitam texturas/buffers de tamanho igual; linhas ainda reconstruem a apresentação quando necessário, reaproveitando geometria científica pronta. O frame anterior continua visível enquanto o próximo é preparado; indicação discreta só aparece após 400 ms, sem interceptar câmera. Cabeçalho da viewport identifica o checkpoint efetivamente mostrado. Reprodução mira 12 estados/s em 1× e aguarda dados quando necessário; não promete FPS independente do hardware.

Também foi removido forceContextLoss do cleanup normal: em hot reload ele deixava o mesmo canvas sem contexto. Materiais, buffers, textures, RAF, observers e workers continuam sendo liberados.

## Arquitetura implementada

NavierDataSource separa serialização, cache e UI. LocalNavierDataSource suporta File.slice e HTTP Range validado. InMemoryDataSource serve testes pequenos. Valores científicos Float32/64 permanecem separados da representação render float32 opcional. Um futuro NavierCoreDataSource por WASM/serviço local/IPC implementará getDatasetMetadata/listCheckpoints/getCheckpoint/getField/getFields/getMetricSeries, diagnostics/cache/close, sem reescrever componentes e engine. Esse backend não foi inventado ou implementado.

Local worker varre assinatura e headers, pula payloads, valida limites/counts/truncamentos e lê campos selecionados. Analysis worker calcula magnitude/curl/geometria com buffers próprios, transfere resultados sem destacar arrays científicos do cache. Cache local LRU usa bytes, chaves step/field/stride, orçamento global 128 MiB por padrão dividido entre datasets. Prefetch tem orçamento adicional por viewport. GPU e arrays temporários não pertencem ao orçamento LRU; o processo pode usar mais RAM que o número mostrado no rodapé.

## Estado dos recursos

**Implementado:** heatmap 2D; cortes XYZ; points GPU; ray-marched volume 3D; vector glyphs; streamlines RK2; campos/derivados genéricos; auto/fixed/manual/global explícito; colormaps e thresholds; timeline uint64 com binary search e slider único; play/pause/loop/speed; duas viewports de comparação; inspector científico em planos; PNG; temas; painéis recolhíveis; métricas existentes e médias dos estados visitados; cancelamento; progress; diagnóstico IO/cache/GPU; error boundary.

**Parcial:** wavefront é nuvem filtrada, não superfície extraída; sparse é ocupação de grade densa amostrada, não formato sparse; comparação apenas visual; inspector não faz seleção volumétrica; global/min/max/média usam LOD, não extrema garantida do arquivo inteiro; cache da preparação não possui deduplicação compartilhada entre viewports. Persistência guarda configurações leves e field/step/mode no hash, sem payloads; câmera é preservada entre frames, não entre sessões.

**Futuro:** isosurfaces; bricks/regiões/LOD persistente; índices persistentes e consultas científicas do core; paginação do diretório; comparação numérica; inspector volumétrico; export vídeo/CSV/alta resolução. Nenhum desses recursos foi apresentado como implementado.

## Verificações realizadas

- npm run build: TypeScript estrito + produção Vite passou; bundle local, sem CDN.
- npm test: 24 testes passaram (parsing, truncamentos, steps uint64, duplicates, campo seletivo, LOD, LRU, derivados, cancelamento durante IO, rejeição de HTTP 200, science-only adapter, XYZ planes, timeline 100.001 descriptors, promoção/deduplicação/cancelamento/bytes do prefetch e chaves visuais).
- cmake --build build-msvc --config Release --target navier_viewer: passou.
- ctest --test-dir build-msvc -C Release --output-on-failure: 63 testes passaram, incluindo o exporter legacy.
- Navegador: heat2d playback/timeline; electromagnetic.data real de 3.606 MiB aberto com IO inicial aproximadamente 8,6 MiB (campo derivado + dependências e prefetch); campo electric_z trocado; fluid3d cloud/volume/slice/streamlines/câmera ortográfica/rotação/comparação; Lorenz células ocupadas; onda amplitude com threshold absoluto; datasets fechados e reabertos.
- Correção de playback: fluid3d completou ciclos passando pelos 31 checkpoints; cloud conservou 1 textura/3 geometrias, volume 2/3; IO estabilizou em cerca de 23,3 MiB após todos os estados e reutilização do cache; mudança de palette não ampliou IO nem exibiu loading. Heat2d também reproduziu sem aviso central. Console sem errors/warnings relevantes na verificação final.
- Memória: validação de orçamentos e contadores, código de lifecycle e ciclos de UI; não foi feito heap snapshot/profiling profundo de driver/GPU. Ausência de crescimento evidente não prova ausência absoluta de leaks.
- PNG tem caminho canvas render + download implementado. O evento de download do navegador embutido não foi capturado pela automação, portanto entrega do arquivo PNG nesse host não está confirmada end-to-end. A captura da interface foi salva separadamente.

## Arquivos grandes / 10 GB

Um fixture virtual de 10 GiB validou endereçamento lendo somente 32 bytes de headers; não foi uma leitura real de 10 GiB. Dataset real de 3.606 MiB foi aberto incrementalmente. Não se promete execução completa/throughput com 10 GB. Antes disso são necessários benchmarking em arquivos reais e hardware diferentes, diretório/LOD/metrics persistidos no core, leitura regional por bricks, batching de headers/ranges, cancelamento CPU cooperativo e controle de concorrência. Startup atual é O(registros), e LOD pode perder detalhes/extrema.

## Dependências e migração

React/ReactDOM para UI; TypeScript para contrato estrito; Three.js para WebGL2, OrbitControls e linhas espessas; Vite/plugin React para bundle e workers; Vitest para testes. package-lock fixa o conjunto instalado. Solvers/core C++ não foram reescritos.

Substituídos/ajustados: viewer/index.html (entry Vite), CMakeLists.txt (legacy path), README.md (novo fluxo), .gitignore (build frontend). Criados: start_viewer.cmd, package.json/package-lock, tsconfig, vite/vitest configs, src módulos, tests e docs. Viewer original copiado para viewer/legacy/; arquivos antigos auxiliares também continuam disponíveis.

## Usar

No projeto: `.\start_viewer.cmd`, depois http://127.0.0.1:4173. Ou `cd viewer`, `npm ci`, `npm run dev` (http://127.0.0.1:5173). Abra Exemplos locais ou selecione juntos arquivo.data e arquivo.data.json. Espaço reproduz/pausa; setas navegam; R/F enquadram. Para a versão de produção: npm run build e npm run preview. Ctrl+C encerra o servidor.

Documentação: viewer/README.md e docs/architecture.md, data-source.md, rendering.md, large-datasets.md.

## Inventário do viewer

- `README.md`
- `docs/architecture.md`
- `docs/data-source.md`
- `docs/large-datasets.md`
- `docs/rendering.md`
- `index.html`
- `legacy/index.html`
- `legacy/viewer_template.hpp.in`
- `legacy/volume3d.js`
- `legacy/volume3d.test.mjs`
- `package-lock.json`
- `package.json`
- `src/analysis/derived.ts`
- `src/analysis/geometry.ts`
- `src/app/App.tsx`
- `src/app/ApplicationBoundary.tsx`
- `src/components/DatasetExplorer.tsx`
- `src/components/FieldExplorer.tsx`
- `src/components/Metrics.tsx`
- `src/components/Timeline.tsx`
- `src/components/VisualizationViewport.tsx`
- `src/data/DataSource.ts`
- `src/data/InMemoryDataSource.ts`
- `src/data/LocalNavierDataSource.ts`
- `src/data/buffers.ts`
- `src/data/cache/ByteLRU.ts`
- `src/data/errors.ts`
- `src/data/types.ts`
- `src/main.tsx`
- `src/parsers/metadata.ts`
- `src/parsers/navierFile.ts`
- `src/parsers/rangeReader.ts`
- `src/parsers/workers/WorkerExecutor.ts`
- `src/parsers/workers/data.worker.ts`
- `src/parsers/workers/executeTask.ts`
- `src/parsers/workers/protocol.ts`
- `src/state/timeline.ts`
- `src/styles/application.css`
- `src/visualization/FramePipeline.ts`
- `src/visualization/GeometryWorker.ts`
- `src/visualization/VisualizationEngine.ts`
- `src/visualization/colormap/colormaps.ts`
- `src/visualization/modes.ts`
- `src/visualization/preparationKey.ts`
- `src/visualization/scalar/paletteShader.ts`
- `src/visualization/scalar/scalarSlice.ts`
- `src/visualization/settings.ts`
- `src/visualization/vector/geometryObject.ts`
- `src/visualization/volume/rayVolume.ts`
- `tests/boundaries.test.ts`
- `tests/compatibility.test.ts`
- `tests/data.test.ts`
- `tests/playback.test.ts`
- `tsconfig.json`
- `viewer_template.hpp.in`
- `vite.config.ts`
- `vitest.config.ts`
- `volume3d.js`
- `volume3d.test.mjs`
