# Desenvolvimento e validação

Branch: feat/navier-storage-engine-v1. C++23, CMake 3.24+, compilador e GoogleTest (instalado ou FetchContent fixado). Adicionado Threads::Threads da biblioteca padrão/toolchain; nenhuma nova biblioteca científica externa. Frontend preserva React/Three/Vite existentes.

```powershell
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Release --parallel 4
ctest --test-dir build-msvc -C Release --output-on-failure -j 4
cmake --build build-msvc --config Debug --parallel 4
ctest --test-dir build-msvc -C Debug --output-on-failure -j 4
cd viewer
npm ci
npm run build
npm test
```

CMake adiciona fontes page_manager/buffer_pool/persistent_bplus_tree/dataset_store; testes storage_v2/wave3d/grayscott3d e integração CLI; benchmarks storage/index_comparison; produtores wave3d_v2/grayscott3d. Opções de warnings-as-errors e sanitizers permanecem disponíveis. Sanitizers não foram executados para esta entrega; não inferir aprovação de ASan/UBSan.

História: baseline inspecionado; formato e IO limitado; árvore paginada e recuperação; consultas/CLI; integração real do viewer; provas 100 MiB e 10 GiB; solvers e extração geométrica; documentação e validação Debug/Release. Sem mudança dos bytes NAVIER01.

Os testes usam oráculos de estrutura, corrupção intencional, limites de alocação, reabertura e invariantes numéricos. Benchmarks não substituem testes. Veja relatório em architectural-evolution-report.md e números em benchmarking.md.

Resultados finais: 89 testes C++ Release (4.35 s), 89 Debug (22.12 s), 28 testes viewer. Builds completos concluídos.
