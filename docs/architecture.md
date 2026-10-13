# Arquitetura atual

Navier transforma a evolução de uma simulação em dados persistentes, indexados e consultáveis. A física permanece nos produtores. O core C++ implementa formatos, armazenamento, páginas e consultas; o viewer solicita dados e produz representações visuais.

```text
Solver externo -> FieldView + métricas -> DatasetStore
                                       | .data NAVIER02 (append-only)
                                       | .nvi (B+ Tree temporal paginada)
                                       | BufferPool -> PageManager
QueryEngine -> metadados / campo selecionado -> CLI C++
 -> transporte local Vite -> NavierCoreDataSource -> worker -> Three.js
```

O índice temporal é persistente. Os índices secundários de métricas são explicitamente construídos em RAM. Não há índice espacial, banco distribuído ou backend C++ permanente nesta versão. Cada solicitação do transporte abre um processo CLI; os limites e a fila evitam concorrência ilimitada, mas há custo de inicialização.

A API antiga CheckpointStore continua lendo e escrevendo NAVIER01. DatasetStore introduz NAVIER02 sem reinterpretar arquivos antigos. Ver [formato](file-format.md), [recuperação](recovery.md) e [roadmap](roadmap.md).

Um mutex serializa métodos de uma instância DatasetStore. Entre instâncias/processos, o chamador deve garantir escritor exclusivo. Não existe bloqueio de arquivo pelo sistema operacional. O pool e a árvore persistente não são APIs concorrentes isoladamente.

Não há promessa de superioridade frente a HDF5/NetCDF. A contribuição é uma implementação explícita de indexação temporal e armazenamento científico com testes e limites auditáveis.
