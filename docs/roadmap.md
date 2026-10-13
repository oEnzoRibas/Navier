# Roadmap e limites

Entregue: B+ Tree RAM robusta, NAVIER02 por campo f32/f64, leitura seletiva limitada, páginas/índice persistente/pool, integridade e recuperação de prefixo, métricas explícitas em RAM, CLI/core adapter, benchmark real 10 GiB, Wave e Gray–Scott 3D, malha de isosuperfície.

1. Durabilidade: fsync/FlushFileBuffers, lock de escritor, testes de encerramento abrupto e protocolo transacional/WAL definido.
2. IO científico: leitura regional verificada já implementada por blocos no payload contíguo, com cache derivado .nvb. Próximos: chunks/bricks nativos, compressão, halo para derivadas e multirresolução persistida. Veja regional-io.md.
3. Consultas: índices de métricas persistentes, consultas compostas, timeline inteiramente paginada e serviço C++ permanente.
4. Manutenção: compactação de versões obsoletas, migração NAVIER01 explícita, integração e comparação controlada com HDF5/NetCDF.
5. Física: convergência de malha/passos, referências independentes; depois Elastic3D, Thermal3D e Quantum3D com critérios próprios. Não implementados nesta etapa.
6. Visualização: diferenças entre checkpoints, seleção 3D/ROI, export científico e análise em resolução integral.

O pedido maior permite priorizar P0/P1 antes de ampliar todos os solvers. Não há compressão, índices espaciais, execução distribuída, retomada exata nos novos exemplos ou garantia contra falta de energia. Tamanho de arquivo grande foi validado; o campo individual continua limitado. Coerência entre múltiplos escritores não é suportada.
