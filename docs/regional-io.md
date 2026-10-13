# Leitura regional verificada

DatasetStore agora oferece `load_region(step, name, FieldRegion)`. O NAVIER02 existente permanece compatível e seus payloads continuam contíguos. A nova leitura trata o campo como blocos de 64 KiB, com checksums num cache derivado. Não é uma implementação de bricks comprimidos nem uma alteração dos bytes científicos.

## API C++

```cpp
navier::storage::DatasetStore store("simulation.data");
navier::storage::FieldRegion plane{
    {0, 0, 24},       // origem na malha original
    {48, 48, 1},     // quantidade de amostras de saída em cada eixo
    {1, 1, 1}        // distância entre amostras na malha original
};
auto field = store.load_region(480, "electric_z", plane);
```

Vetores devem ter o mesmo rank do campo, 1–8. X varia mais rápido. `count` é a forma da saída, não o comprimento do intervalo antes de aplicar stride. O último índice é `origin+(count−1)*stride`. Bounds, overflow, dimensões, orçamento de saída e comprimento do arquivo são verificados antes de alocar o resultado. Float32/64 e seus bits, inclusive −0/NaN, são preservados. A forma do Field retornado é count; conserve a requisição e o descritor original para posicionamento físico.

O orçamento de saída usa max_field_bytes (256 MiB por padrão). Há também um buffer de um bloco e uma tabela limitada a 16 MiB, além de metadados. Não é um limite de memória total do processo. A tabela limita o campo original a até 256 GiB nesta implementação; um campo grande pode ser lido regionalmente sem alocar seu volume completo.

## Integridade e primeira leitura

O NAVIER02 guarda um CRC do campo inteiro. Na primeira leitura regional de cada campo/registro, o core varre esse campo em blocos e confere o CRC original antes de publicar a tabela. Isso valida inclusive bytes fora da região pedida. Não carrega o campo inteiro em RAM.

Depois, carrega e verifica apenas os blocos envolvidos na região, reutilizando o último bloco ao percorrer as amostras. Ordenação X-first favorece cortes Z e regiões contíguas. Um corte X ou stride disperso pode tocar todos os blocos: nessa situação a memória e a transferência HTTP diminuem, mas o IO do .data pode não diminuir.

Validação regional certifica os blocos lidos. Corrupção em outros blocos continua sendo responsabilidade de `validate full`. CRC32 detecta corrupção acidental, não autentica conteúdo. Não adiciona garantia contra falta de energia ou múltiplos escritores.

## Cache .nvb

Ao lado de `file.data`, o diretório `file.data.nvb` guarda uma tabela por campo acessado. É derivado, ignorado pelo Git e pode ser descartado para reconstrução. Os arquivos científicos não são modificados. Nome: `<offset>-<bytes>-<crc>.nvc`. Identidade do dataset, geração do registro, offset, comprimento e CRC do campo vinculam a tabela ao payload correto. Substituições de step usam a nova localização; append de outro checkpoint não invalida caches antigos.

Cabeçalho de 128 bytes, little-endian:

|Offset|Conteúdo|
|---|---|
|0|NVBLOCK1, 8 bytes|
|8,12|versão u32=1, tamanho de bloco u32=65536|
|16,24|identidade do dataset, dois u64|
|32,40,48|geração do registro, offset absoluto e bytes do campo, u64|
|56|CRC original do campo, u32|
|64|quantidade de blocos, u64|
|72|CRC da tabela, u32|
|124|CRC dos primeiros 124 bytes, u32|

Após o cabeçalho: CRC32 u32 de cada bloco; último bloco pode ser menor. Reservados zero. Tamanho máximo da tabela 16 MiB. Cabeçalho/tabela danificados são reconstruídos validando novamente o campo completo. Publicação usa arquivo temporário distinto e rename; leitores concorrentes podem calcular a mesma tabela. Arquivos temporários abandonados e caches de versões antigas não têm coleta automática nesta etapa.

## CLI e proxy

```powershell
.\build-msvc\Release\navier.exe extract-region runs/showcase/interference3d.data 480 electric_z 0,0,24 48,48,1 1,1,1 plane.raw
.\build-msvc\Release\navier.exe region-info-json runs/showcase/interference3d.data 480 electric_z 0,0,24 48,48,1 1,1,1
```

CLI/proxy limitam a resposta regional a 64 MiB. `region-info-json` reporta bytes de resultado, bytes lidos do .data naquela chamada e verificação dos blocos. Esse contador exclui o índice temporal e arquivos .nvb; não equivale a bytes físicos de disco com caches do SO.

O proxy oferece GET `/region/step/field/origin-csv/count-csv/stride-csv` apenas para datasets do catálogo e mantém fila/cancelamento/timeout. Em campos muito grandes, a primeira construção pode exceder o timeout HTTP de 60 s: prepare o cache com `region-info-json` pela CLI antes de usar a GUI.

## Viewer

NavierCoreDataSource implementa getRegion e consulta dimensões por checkpoint, mantendo cache regional por step/campo/origem/count/stride. Cortes X/Y/Z usam leitura parcial para campos nativos e módulos vetoriais, sem carregar a variável inteira no browser. LOD solicita amostras esparsas ao core. A UI indica leitura regional; inspector, posição e extensão do plano usam coordenadas da malha original. Métricas dos estados visitados reiniciam ao alterar corte para não misturar regiões.

Rotacional ainda usa campo completo, pois derivadas exigem halo. Sobreposições vetoriais também usam caminho completo. Volume e isosuperfície continuam lendo o campo inteiro. O adapter legado NAVIER01 conserva seu caminho anterior. Não foram implementados streaming por bricks, compressão, seleção ROI com mouse ou índices espaciais.

## Medidas reais

No arquivo de 10 GiB, step 17913, field_1 de 1028×512 float32 (2105344 bytes), ROI 16×16 em (32,32): saída 1024 bytes. Primeira chamada leu 2171123 bytes do .data para criar/verificar o cache. Chamada seguinte leu **65779 bytes**, incluindo diretório do checkpoint: aproximadamente 32 vezes menos que o payload completo. Não é benchmark de latência de disco frio.

Interferência 48³, electric_z: corte Z 48×48 devolveu **9216 bytes**, contra 442368 bytes do campo inteiro (48 vezes menos na resposta). Primeira chamada: 509478 bytes lidos do .data; outra fatia com cache pronto: 67110 bytes. CSVs em benchmark-results/regional-io.csv.

## Verificação

107 testes C++ passaram em Release e Debug, incluindo regiões estridadas, X/Y/Z, rank 4, bits float64, bounds/overflow/budgets, corrupção de bloco, validação completa no primeiro acesso, cache corrompido, substituição e leitores concorrentes. Integração CLI confere binário e JSON regionais. Viewer passou 37 testes, incluindo leitura de uma região de campo maior que o limite de transporte integral, cache, bounds e posicionamento dos planos. Browser conferiu cortes, mudança de posição e módulo vetorial regional sem erros de console.

Durabilidade, daemon C++ permanente, compressão/compactação, índices persistentes de métricas e restart completo continuam no roadmap.
