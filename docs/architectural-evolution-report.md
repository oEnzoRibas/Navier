# Evolução arquitetural do Navier — entrega 2026-10-12

## Resultado

Foi implementada uma primeira geração coerente de armazenamento científico por campos e indexação temporal persistente, priorizando P0/P1 do pedido. NAVIER02 soma descritores, float32/float64, dimensões/unidades por checkpoint, métricas, leitura seletiva, páginas, pool limitado, checksums e recuperação de prefixo. A GUI usa o core real, exibe métricas salvas e extrai isosuperfícies como malhas. A API NAVIER01 permanece compatível.

Não foram implementadas todas as cem seções do pedido. Compressão/chunking/ROI, índices secundários persistentes, WAL/fsync, compactação, migração automática, retomada exata, Quantum3D/Elastic3D/Thermal3D e análise científica avançada ficam explicitamente no roadmap. O formato registra restart, mas os produtores novos só salvam analysis.

## Evidências de validação

- C++ Release: build completo MSVC com warnings-as-errors; **89/89 testes**, 4.35 s.
- C++ Debug: build completo; **89/89 testes**, 22.12 s.
- Viewer: TypeScript estrito + build de produção; **28/28 testes**, 5 arquivos.
- Persistência: árvore de 50000 chaves com múltiplos níveis e reabertura sob pools pequenos; stress RAM 100000 operações com oráculo; corrupção, diretórios malformados, campos e budgets, dirty index, truncamentos e concorrência na mesma instância.
- Dataset real **10759016032 bytes**, 2560 checkpoints, índice 131072 bytes. Abertura sem varrer registros. **validate full passou**, incluindo todos os campos de versões históricas, em blocos de 64 KiB.
- Dados reais gerados de Wave3D v2 (17 checkpoints) e Gray–Scott 3D (41); Gray passou validação completa de payloads.
- Browser: dataset 10 GiB abriu, navegou ao último step 17913 e trocou campo; cache permaneceu dentro de 128 MiB. Não significa que um campo individual de 10 GiB seja suportado. Gray: reprodução percorreu os 41 estados; malha no passo 4000 (species_v, isovalor 0.1), sem erros de console. Wave v2: energia em volume no passo 160; NAVIER01 heat2d abriu e renderizou por HTTP Range.

ASan/UBSan e testes de queda de energia não foram executados nesta entrega. Resultados de benchmark são medidas locais sem cache de SO controlado. Nenhuma alegação de superioridade HDF5/ParaView.

## Arquitetura, formato e operações

Consulte [arquitetura](architecture.md), [bytes do formato](file-format.md), [armazenamento](checkpoint-storage.md), [árvore RAM](bplus-tree.md), [índice persistente](persistent-index.md), [páginas/pool](buffer-pool.md), [recuperação](recovery.md), [consultas](query-engine.md), [integração GUI](viewer.md).

Separação: solver externo -> core C++ -> query/CLI -> proxy local -> DataSource -> worker -> GPU. O frontend não reimplementa B+ Tree nem decodifica páginas NAVIER02. O transporte usa processos CLI limitados; não é um daemon C++ persistente. O índice .nvi é derivado reconstruível. Healthy open reusa páginas, enquanto índice ausente/inconsistente exige varredura de diretórios (sem payload).

Salvamento: diretório e campos em blocos, CRC por campo, rodapé confirmado, índice dirty/commit, slot alternado. Falha de append exige reabrir. Cauda incompleta é preservada e bloqueia novas escritas até repair-tail explícito. Sem garantia ACID/power-loss; exclusividade entre processos fica com o chamador.

## Simulações

[Simulações](simulations.md) descreve equação, método, campos, validação e limites de cada produtor existente e dos novos Wave/Gray. Campos de EM chamados velocity_* são Poynting no legado. Isosuperfície usa marching tetrahedra real; pontos de frente de onda continuam identificados como pontos.

Novos exemplos para executar novamente:

```powershell
.\build-msvc\Release\navier_wave3d_v2.exe wave_nova.data 32 160 10
.\build-msvc\Release\navier_grayscott3d.exe gray_novo.data 32 32 32 4000 100
.\start_viewer.cmd
```

Escolha nomes novos. No catálogo, abra o .data; NAVIER02 dispensa sidecar. Para Gray, selecione species_v, Isosuperfície e um isovalor dentro do intervalo do estado (por exemplo 0.1). Rotação/zoom preservam a câmera entre checkpoints.

## Benchmarks

[Resultados completos](benchmarking.md) e CSVs em benchmark-results/. Arquivo 10 GiB: escrita 133.77 s (~76.71 MiB/s), abertura 222.05 ms, descritor 0.0904 ms, campo 7.0384 ms, range de 1000 descritores 58.31 ms. Tempo de geração excluído; checksum/serialização/IO/índice medidos juntos. Hash venceu igualdade e vetor ordenado venceu ranges no comparativo RAM; B+ Tree tem utilidade específica, não vantagem universal.

## Build, dependências e comandos

[Desenvolvimento](development.md) contém comandos Debug/Release/frontend, opções e mudanças CMake. Adicionado Threads::Threads da toolchain; nenhum novo pacote científico. GoogleTest continua instalado ou fixado por FetchContent. Novos targets: navier_storage_benchmark, navier_index_comparison, navier_wave3d_v2, navier_grayscott3d. Novo teste de integração CLI.

```powershell
.\build-msvc\Release\navier.exe info runs/storage-v1/grayscott3d.data
.\build-msvc\Release\navier.exe fields runs/storage-v1/grayscott3d.data 4000
.\build-msvc\Release\navier.exe extract runs/storage-v1/grayscott3d.data 4000 species_v field.raw
.\build-msvc\Release\navier.exe validate runs/storage-v1/grayscott3d.data full
.\build-msvc\Release\navier.exe query runs/storage-v1/grayscott3d.data max_v 0.1 0.4
.\tools\generate_large_dataset.ps1 -FullValidation
```

query constrói índice de métricas em RAM explicitamente naquela invocação. Dados .data/.nvi ficam fora do Git. Não foi publicado PR nem alterado remoto.

## Limites e próximos passos

[Roadmap](roadmap.md) prioriza durabilidade e exclusão mútua, depois campos chunked/regionais, consultas persistentes e compactação; só então amplia física. Core aceita ranks 1–8, viewer 1–3; carga core padrão 256 MiB, transporte binário 64 MiB, timeline aproximadamente 64 MiB. Dimensões variam por checkpoint, mas campo ainda é lido inteiro. Metadados JSON são convenção opaca do core.

## Inventário de arquivos

Status do Git ao preparar o relatório: M = modificado, ?? = criado. Arquivos de playback já estavam alterados no início desta etapa e foram preservados. Não inclui os próprios arquivos de relatório criados após a captura.

```text
 M .gitignore
 M CMakeLists.txt
 M README.md
 M docs/architecture.md
 M examples/solvers/run_options.hpp
 M examples/solvers/wave3d.hpp
 M include/navier/index/bplus_tree.hpp
 M include/navier/navier.hpp
 M include/navier/storage/checkpoint_store.hpp
 M src/storage/checkpoint_store.cpp
 M tools/navier_cli.cpp
 M viewer/README.md
 M viewer/docs/architecture.md
 M viewer/docs/data-source.md
 M viewer/docs/rendering.md
 M viewer/src/analysis/geometry.ts
 M viewer/src/app/App.tsx
 M viewer/src/components/DatasetExplorer.tsx
 M viewer/src/components/FieldExplorer.tsx
 M viewer/src/components/Timeline.tsx
 M viewer/src/components/VisualizationViewport.tsx
 M viewer/src/parsers/workers/data.worker.ts
 M viewer/src/parsers/workers/executeTask.ts
 M viewer/src/parsers/workers/protocol.ts
 M viewer/src/styles/application.css
 M viewer/src/visualization/VisualizationEngine.ts
 M viewer/src/visualization/modes.ts
 M viewer/src/visualization/settings.ts
 M viewer/vite.config.ts
?? benchmarks/index_comparison.cpp
?? benchmarks/storage_benchmark.cpp
?? docs/adr/
?? docs/architectural-evolution-report.md
?? docs/baseline-v1.md
?? docs/benchmark-results/
?? docs/benchmarking.md
?? docs/bplus-tree.md
?? docs/buffer-pool.md
?? docs/checkpoint-storage.md
?? docs/development.md
?? docs/file-format.md
?? docs/persistent-index.md
?? docs/query-engine.md
?? docs/recovery.md
?? docs/roadmap.md
?? docs/simulations.md
?? docs/viewer.md
?? examples/grayscott3d_demo.cpp
?? examples/solvers/grayscott3d.hpp
?? examples/wave3d_v2_demo.cpp
?? include/navier/format/
?? include/navier/index/persistent_bplus_tree.hpp
?? include/navier/query/
?? include/navier/storage/buffer_pool.hpp
?? include/navier/storage/dataset_store.hpp
?? include/navier/storage/field.hpp
?? include/navier/storage/page.hpp
?? include/navier/storage/page_manager.hpp
?? src/storage/buffer_pool.cpp
?? src/storage/dataset_store.cpp
?? src/storage/page_manager.cpp
?? src/storage/persistent_bplus_tree.cpp
?? tests/dataset_cli_test.cmake
?? tests/grayscott3d_test.cpp
?? tests/storage_v2_test.cpp
?? tests/wave3d_test.cpp
?? tools/dataset_cli.hpp
?? tools/generate_large_dataset.ps1
?? viewer/coreTransport.ts
?? viewer/docs/verification.md
?? viewer/src/analysis/isosurface.ts
?? viewer/src/data/NavierCoreDataSource.ts
?? viewer/src/visualization/FramePipeline.ts
?? viewer/src/visualization/preparationKey.ts
?? viewer/src/visualization/scalar/isosurfaceObject.ts
?? viewer/tests/core-source.test.ts
?? viewer/tests/playback.test.ts
```

ADRs aceitas em adr/ documentam árvore temporal, append-only, páginas/pool, campos, fronteira DataSource, analysis/restart e IO seletivo, com alternativas e consequências.
