# Benchmarks medidos

Máquina: Windows 10 Pro, Ryzen 5 3600X (6 cores), aproximadamente 32 GiB RAM, MSVC 19.50.35727.0 / Visual Studio 18 2026, Release. Node 24.12.0. Um ensaio de cada arquivo; caches do sistema não foram controlados. Não são latências garantidas ou medidas de disco frio.

| Medida | 100 MiB | 10 GiB |
|---|---:|---:|
|Bytes reais .data|105729592|10759016032|
|Checkpoints|200|2560|
|Bytes .nvi|16384|131072|
|Escrita total ms|341.59|133766.98|
|Throughput MiB/s|295.18|76.71|
|Abertura ms|6.21|222.05|
|Registros varridos na abertura|0|0|
|Descritor exato ms|0.0849|0.0904|
|Leitura de um campo ms|0.942|7.0384|
|Intervalo (até 1000) ms|14.57 (200)|58.31 (1000)|
|Busca aquecida us|55.23|54.59|

Abertura leu 346 bytes do .data (superblocos/metadados), além de páginas do índice contabilizadas pelo pool. Geração sintética fora do relógio; checksum, serialização, IO e índice dentro do tempo de escrita conjunto. Não há decomposição cronometrada dessas parcelas. Campos float32 de senos repetidos, duas variáveis, dimensões X variáveis e steps irregulares; não é um benchmark físico nem compressão. Campo completo de alguns MiB, não um campo de 10 GiB. Dataset de 10 GiB passou validate full de todos os campos históricos.

Comparação RAM: 1000/10000/100000/1000000 entradas, três ensaios, mesma sequência. Cinco estruturas: linear, vetor ordenado, std::map, unordered_map, B+ Tree própria. Medianas com 1 milhão, ns por operação:

|Estrutura|Construção/entrada|Exata|Range|Travessia/entrada|Inserção dinâmica|
|---|---:|---:|---:|---:|---:|
|Linear|4.57|398480|2409056|92.57|24865|
|Vetor ordenado|83.19|491.41|887.50|3.11|370115|
|std::map|685.60|765.23|8131.25|92.27|2061|
|Hash|162.75|100.78|38666425|127.31|462.50|
|B+ Tree|279.24|286.33|3243.75|21.93|1258.59|

Hash venceu igualdade; vetor venceu range neste ensaio. Bulk-sort do vetor não equivale à construção incremental; hash/linear ordenam para travessia. Há overhead de adapters std::function, apenas 256 probes exatas/16 ranges/128 inserts dinâmicos, ordem fixa e caches não controlados. Não generalizar. Sem comparação HDF5 implementada.

```powershell
.\build-msvc\Release\navier_storage_benchmark.exe runs/storage-v1/benchmark100m.data 200 256 256 2
.\build-msvc\Release\navier_storage_benchmark.exe runs/storage-v1/benchmark10g.data 2560 1024 512 2
.\build-msvc\Release\navier_index_comparison.exe > index-comparison.csv
.\build-msvc\Release\navier.exe validate runs/storage-v1/benchmark10g.data full
.\tools\generate_large_dataset.ps1
```

Use novo nome se o dataset existe. Arquivos .data/.nvi não entram no Git; CSVs reproduzíveis ficam em benchmark-results/.
