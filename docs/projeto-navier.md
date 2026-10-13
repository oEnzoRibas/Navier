# Navier: da ideia inicial ao armazenamento científico

Este documento apresenta a evolução e o funcionamento do projeto com base na implementação atual. A evolução é descrita por etapas técnicas; não pretende reconstruir datas ou decisões que não estejam registradas no repositório.

## 1. A ideia inicial

Uma simulação calcula estados sucessivos: temperaturas, velocidades, pressões, concentrações ou campos eletromagnéticos. Se esses estados existem apenas na memória, encerrar o programa significa perder a possibilidade de inspecionar sua evolução sem executar tudo novamente.

A ideia do Navier é transformar esses estados em **checkpoints persistentes e consultáveis**. Em vez de salvar somente a imagem final, o produtor salva dados numéricos associados a um passo e a um tempo. Depois, outra aplicação pode encontrar um estado, selecionar uma variável e visualizá-la.

As perguntas que motivam a arquitetura são simples:

- Qual era o estado no passo 480?
- Quais checkpoints existem entre os passos 200 e 600?
- Posso ler somente a temperatura, sem carregar as velocidades?
- Posso mostrar um corte de um volume sem transferir o volume inteiro?
- Como perceber um arquivo incompleto ou um campo corrompido?

O projeto começou com uma B+ tree em memória e um arquivo de vetores. Evoluiu para campos científicos descritos dentro do próprio arquivo, uma B+ tree temporal persistente, verificação de integridade e uma interface gráfica.

## 2. O que o Navier faz

O Navier é um motor experimental de armazenamento e indexação de estados numéricos, acompanhado de ferramentas e um visualizador local. O solver produz os números; o Navier organiza sua persistência e consulta; o viewer transforma esses números em representações visuais.

```text
Produtor / solver C++
    calcula o próximo estado
    entrega campos + métricas + step + time
             |
             v
       DatasetStore
        |    |    |
        |    |    +-- .data.nvb/ : cache de checksums para regiões
        |    +------- .data.nvi  : índice temporal persistente
        +------------ .data      : estados científicos e metadados
             |
             v
        CLI C++ real
             |
       transporte HTTP local
             |
      React + workers + Three.js
             |
      animação / cortes / volume / linhas
```

O core não depende da GUI para salvar ou consultar arquivos. A GUI não implementa uma segunda B+ tree em JavaScript: para NAVIER02, usa o core C++ por meio da CLI.

## 3. A evolução do projeto

| Etapa | O que foi acrescentado | O problema resolvido |
| --- | --- | --- |
| Estrutura de dados | B+ tree genérica em RAM | Busca ordenada, inserções e intervalos |
| Checkpoints legados | `CheckpointStore` e NAVIER01 | Persistência de `step`, `time` e vetor de doubles |
| Exemplos numéricos | Produtores de calor, fluidos e outros fenômenos | Uso concreto com dados de simulação |
| Visualização | GUI 2D/3D com timeline e câmera | Inspeção e animação dos estados gravados |
| Armazenamento por campos | `DatasetStore` e NAVIER02 | Tipos, dimensões, unidades e leitura de uma variável |
| Índice persistente | Páginas, buffer pool e `.nvi` | Reabertura saudável sem varrer todos os registros |
| Integridade e recuperação | CRCs, dois slots de superbloco e rodapé de commit | Detecção de corrupção e recuperação de registros completos |
| Consultas de métricas | Índices secundários explícitos em RAM | Localizar checkpoints por um diagnóstico escalar |
| Leitura regional | `load_region` e cache `.nvb` | Ler amostras e cortes com memória de saída limitada |

NAVIER01 e NAVIER02 continuam coexistindo. A API nova não reinterpreta o formato antigo e não existe migração automática.

## 4. Os conceitos usados pelo core

**Step** é uma chave inteira sem sinal de 64 bits. **Time** é o tempo numérico associado ao estado, armazenado como float64 finito. A árvore temporal é ordenada por step, não por time; passos irregulares são permitidos.

Um **campo** tem nome, tipo escalar, dimensões e unidade. Exemplos: `temperature` em uma grade 2D, `electric_z` em uma grade 3D, ou uma componente de velocidade. O formato permite rank de 1 a 8 e valores float32 ou float64. Os campos não precisam manter a mesma forma em todos os checkpoints.

Uma **métrica** é um diagnóstico escalar finito, com nome e unidade: máximo de amplitude, energia integrada ou concentração média. Ela fica no diretório do checkpoint e pode ser consultada sem carregar os campos.

`FieldView` empresta um span do produtor durante `save`; o produtor mantém os buffers vivos até a chamada retornar. `Field` contém um vetor próprio e é usado como resultado de leitura. `CheckpointDescriptor` reúne metadados, descritores de campos e métricas; `ScientificCheckpoint` reúne também os valores dos campos.

`analysis` indica um estado para inspeção. `restart` é uma declaração do produtor: salvar esse rótulo não garante que todos os estados internos e parâmetros necessários à retomada foram incluídos. Os novos exemplos salvam estados de análise e não implementam retomada exata.

## 5. O papel dos arquivos

Para um dataset chamado `simulacao.data`, a organização atual é:

```text
simulacao.data       dados científicos NAVIER02
simulacao.data.nvi   B+ tree temporal paginada
simulacao.data.nvb/  tabelas derivadas de CRC por bloco
```

O `.data` é a fonte dos estados. O `.nvi` é um índice derivado reconstruível. O diretório `.nvb` também é derivado e pode ser reconstruído. Durante reconstrução/publicação podem existir arquivos temporários.

Produtores legados usam NAVIER01 e normalmente um `.data.json` com informações que o vetor sozinho não descreve. NAVIER02 inclui descritores científicos e metadados no `.data`, dispensando esse sidecar para sua interpretação pelo core.

## 6. Por que um binário estruturado

Um arquivo binário estruturado é uma sequência de bytes com um contrato de interpretação: assinatura, versão, tamanhos, tipos, offsets e regras de validação. Não é um despejo da memória do programa.

Navier serializa os números explicitamente em **little-endian**, usando bits IEEE 754 para floats. Não grava ponteiros, objetos STL ou padding de structs. Isso evita depender da disposição que um compilador escolheu para objetos C++.

Por exemplo, o inteiro `0x01020304` ocupa os bytes `04 03 02 01`. Um offset indica uma posição no arquivo; não é um endereço de RAM. Produtos de dimensões e somas de tamanhos são verificados para evitar overflow antes das alocações.

O formato usa CRC32 IEEE, polinômio `0xedb88320`, para detectar corrupção acidental. CRC não fornece autenticação nem proteção criptográfica contra alterações intencionais.

## 7. NAVIER01: o ponto de partida

O formato legado começa com a assinatura de 8 bytes `NAVIER01`. Cada registro contém:

| Conteúdo | Tamanho |
| --- | ---: |
| Step, u64 | 8 bytes |
| Bits do tempo, float64 | 8 bytes |
| Quantidade de valores, u64 | 8 bytes |
| Valores float64 | 8 × quantidade |

Assim, um registro com 100 doubles ocupa `24 + 100 × 8 = 824` bytes. O formato não descreve nomes, unidades ou dimensões de cada variável.

Na abertura, `CheckpointStore` percorre os cabeçalhos, calcula os comprimentos, pula os payloads e reconstrói uma B+ tree em memória. A abertura cresce com o número de registros, mesmo que os valores não sejam carregados. O formato legado não possui os checksums e mecanismos de recuperação do NAVIER02.

## 8. NAVIER02: visão geral do arquivo

```text
Offset 0       região de superbloco: 4096 bytes
                 slot A: 128 bytes em 0
                 slot B: 128 bytes em 256
Offset 4096    metadados do dataset: até 1 MiB
               padding para alinhar o início dos registros em 8 bytes
               checkpoint 1
               checkpoint 2
               ...
```

Os metadados são bytes opacos para o formato; os exemplos usam JSON para descrever método, parâmetros e condições. O core verifica tamanho e CRC, mas não exige um esquema físico universal.

### 8.1. Os dois slots do superbloco

Cada slot tem 128 bytes e descreve a versão confirmada do dataset. A abertura seleciona o slot válido com maior geração. As atualizações alternam os slots; eles não são duas cópias do arquivo inteiro.

| Offset no slot | Tipo | Significado |
| --- | --- | --- |
| 0 | 8 bytes | Assinatura `NAVIER02` |
| 8 | u32 | Versão 2 |
| 12 | u32 | Marcador `0x01020304` |
| 16, 20 | u32, u32 | Página 4096 e flags 0 |
| 24 | u64 | Geração |
| 32 | u64 | Fim confirmado |
| 40 | u64 | Início dos registros |
| 48, 56 | u64, u64 | Offset e tamanho dos metadados |
| 64 | u32 | CRC dos metadados |
| 72, 80 | u64, u64 | Identidade do dataset |
| 88 | u64 | Raiz temporal espelhada |
| 96 | u64 | Quantidade de steps ativos |
| 104, 112 | u64, u64 | Primeiro e último step |
| 124 | u32 | CRC dos primeiros 124 bytes |

Os bytes reservados são zero. A identidade distingue datasets; a geração liga uma versão do `.data` ao seu índice. A raiz espelhada não substitui essas verificações.

### 8.2. O registro de checkpoint

```text
[ cabeçalho: 64 bytes ]
[ diretório: descritores de campos e métricas ]
[ payload do campo 1 ][ payload do campo 2 ] ...
[ rodapé: 16 bytes ]
```

| Offset no cabeçalho | Tipo | Significado |
| --- | --- | --- |
| 0 | 8 bytes | `NVCPT002` |
| 8 | u64 | Comprimento total do registro |
| 16 | u64 | Step |
| 24 | float64 | Tempo finito |
| 32 | u64 | Geração sequencial |
| 40, 44 | u32, u32 | Quantidades de campos e métricas |
| 48 | u32 | Tamanho do diretório |
| 52 | u32 | 0 para analysis; 1 para restart |
| 56 | u32 | CRC do diretório |
| 60 | u32 | CRC dos primeiros 60 bytes |

Há de 1 a 1024 campos, até 1024 métricas e até 1 MiB de diretório. O rodapé contém `NVCOMMIT` e o comprimento total u64. Ele permite verificar se o registro possui o término esperado, mas não equivale sozinho a uma transação resistente a falta de energia.

### 8.3. O diretório e os payloads

Cada campo descreve:

- Nome e unidade, cada um com comprimento u16 e até 255 bytes.
- Tipo u8: 1 para float32 ou 2 para float64.
- Rank u8 e dimensões u64 positivas.
- Codec u16: atualmente 0, dados raw.
- Offset relativo ao início do registro.
- Tamanhos armazenado e bruto, em bytes.
- CRC do payload.

As métricas armazenam nome, unidade e valor float64 finito. Os campos são contíguos, sem compressão ou bricks nativos. Codec e tamanho bruto deixam espaço para evolução, mas não significam que compressão já exista.

Para dimensões `(NX, NY, NZ)`, X varia mais rápido:

```text
indice(x,y,z) = x + NX * (y + NY * z)
offset_do_valor = inicio_do_registro + offset_relativo_do_campo
                  + indice(x,y,z) * bytes_por_escalar
```

Uma componente float32 de uma grade 48³ ocupa `48 × 48 × 48 × 4 = 442368` bytes. Cada componente vetorial pode ser um campo separado. A leitura usa o descritor para encontrar e decodificar somente a variável solicitada.

## 9. Por que usar B+ trees

O índice temporal associa:

```text
step -> localização do registro (offset, comprimento)
```

Os campos grandes ficam fora da árvore. Essa separação mantém o índice pequeno, mesmo quando cada checkpoint ocupa muitos megabytes.

Uma B+ tree é uma árvore balanceada com muitos filhos por nó. Nós internos guardam separadores e referências aos filhos; folhas guardam as chaves e seus valores. Todas as folhas têm a mesma profundidade e são ligadas em ordem.

Uma árvore conceitual pequena seria:

```text
                    [ 30 | 60 ]
                   /     |     \
       [10,20] -------- [30,40,50] -------- [60,70,80]
          folha             folha              folha
```

Cada separador é o mínimo da subárvore à direita. A chave 30 desce para o filho do meio; 60 desce para o último. Os traços entre folhas representam links usados para percorrer intervalos.

O exemplo ilustra a navegação; não representa as capacidades reais dos nós do projeto.

### 9.1. Busca exata

Nos nós internos, `upper_bound` escolhe o filho, enviando igualdade para a direita. Na folha, `lower_bound` localiza a posição candidata e verifica se a chave existe. No índice temporal, o valor encontrado permite saltar diretamente ao registro no `.data`.

### 9.2. Inserção e split

A inserção desce à folha e mantém suas chaves ordenadas. Uma chave existente recebe outro valor; não aumenta a quantidade de chaves.

Quando uma folha excede sua capacidade, ela é dividida em duas. A nova folha direita herda o link para a antiga sucessora, a esquerda passa a apontar para a direita e a primeira chave da direita sobe como separador.

Se um nó interno também exceder sua capacidade, um separador central é promovido e seus filhos são distribuídos entre dois nós. Se a raiz se dividir, cria-se uma nova raiz. Por isso a árvore cresce em altura sem deixar folhas em profundidades diferentes.

### 9.3. Consulta por intervalo

Para buscar `[200,600]`, a árvore desce uma vez até a folha que pode conter 200. Depois percorre entradas e folhas seguintes até ultrapassar 600. Isso evita repetir uma busca desde a raiz para cada resultado.

Com capacidades fixas, busca e inserção têm custo `O(log N)`; intervalo tem custo `O(log N + K)`, sendo K a quantidade de resultados. Em disco, o número de páginas visitadas também importa.

A B+ tree não vence todas as alternativas: hash pode ser melhor para igualdade, vetor ordenado para dados estáticos, e steps perfeitamente regulares com registros fixos podem permitir endereço calculado. Seu valor aqui é combinar ordem, intervalos e inserções com páginas persistentes.

## 10. As B+ trees que existem no projeto

| Estrutura | Onde vive | Chave e valor | Uso |
| --- | --- | --- | --- |
| `BPlusTree<Key,Value,Order,Compare>` | RAM | Genéricos | Estrutura inicial, índice legado e índices de métricas |
| `PersistentBPlusTree` | `.nvi`, com cache em RAM | u64 step → offset/comprimento | Índice temporal NAVIER02 |
| Árvores de métricas do `QueryEngine` | RAM | (valor, step) → step | Intervalos de diagnósticos escalares |

A implementação genérica é própria; não delega a árvore a `std::map`. O map aparece como oráculo nos testes. `Order` é o máximo de filhos de um nó interno: a configuração padrão 32 permite até 31 chaves por nó. A posse dos nós usa `unique_ptr`; o link `next` entre folhas é uma referência não proprietária.

`validate` confere ordenação, ocupação, separadores, quantidade, profundidade das folhas e encadeamento. As APIs atuais não implementam remoção individual de chaves com rebalanceamento.

## 11. A B+ tree persistente e as páginas

O `.nvi` é dividido em páginas de **4096 bytes**. Um `PageId` é um número de página:

```text
offset_da_pagina = PageId * 4096
```

A página 0 guarda o cabeçalho do índice. Os nós usam outras páginas. O arquivo nunca salva um ponteiro C++; os filhos e a próxima folha são PageIds u64.

### 11.1. Cabeçalho do índice

| Offset na página 0 | Conteúdo |
| --- | --- |
| 0 | `NVINDEX1` |
| 8, 12 | Versão u32 e tamanho da página u32 |
| 16, 24 | Raiz u64 e quantidade de chaves u64 |
| 32, 40 | Geração u64 e marcador dirty u64 |
| 48, 56 | Identidade do dataset, dois u64 |
| 4092 | CRC dos primeiros 4092 bytes |

### 11.2. Página de nó

| Offset | Conteúdo |
| --- | --- |
| 0 | `BPT1` |
| 4 | Tipo u8: 1 folha, 0 interno |
| 6 | Quantidade de chaves u16 |
| 8 | Próxima folha, PageId u64; zero indica término |
| 32 | Início das entradas |
| 4092 | CRC da página |

Uma folha guarda entradas `(step, offset, length)` de 24 bytes:

```text
capacidade = floor((4092 - 32) / 24) = 169 entradas
```

Um interno guarda `child0` de 8 bytes seguido de pares `(separator, child)` de 16 bytes:

```text
capacidade = floor((4092 - 32 - 8) / 16) = 253 separadores
                                               254 filhos
```

Fora da raiz, a ocupação mínima é de 85 entradas em folhas e 126 chaves internas. Splits são incrementais: inserir uma chave não exige reescrever toda a árvore. A leitura confere CRC, tipos, limites, ordenação e referências; a validação estrutural também procura ciclos e links inconsistentes. A descida é limitada a 64 níveis para impedir laços em arquivos corrompidos.

### 11.3. PageManager e BufferPool

`PageManager` aloca, lê e escreve páginas inteiras e verifica se o arquivo tem um número integral de páginas. `BufferPool` mantém uma quantidade limitada de páginas em RAM, com política LRU.

Uma página pinned está em uso por um guard e não pode ser expulsa. Uma página dirty foi alterada e precisa ser escrita. Ao expulsar uma página dirty não fixada, o pool a escreve antes de removê-la. Se todas estiverem pinned, a operação falha em vez de invalidar referências ativas. O guard libera o pin ao sair do escopo.

O padrão do DatasetStore é 64 páginas de cache: 256 KiB de conteúdo de páginas, além das estruturas de controle. Esse cache pertence à instância aberta; o transporte atual cria processos CLI por requisição e não mantém um único pool vivo entre todas as requisições da GUI.

## 12. O caminho de uma gravação

`DatasetStore::save` valida campos, métricas, tamanhos e parâmetros. Calcula checksums e escreve os campos em blocos de 64 KiB, sem montar uma cópia monolítica do checkpoint. Há duas passagens sobre os valores: cálculo de CRC e serialização/escrita.

A ordem principal é:

1. Acrescentar cabeçalho, diretório, payloads e `NVCOMMIT` no `.data`.
2. Executar flush do arquivo de dados.
3. Marcar o índice dirty, inserir/substituir a localização e fazer commit do índice.
4. Atualizar o slot alternado do superbloco com a nova geração, fim confirmado e resumo temporal.

No índice, `begin_update` grava o marcador dirty antes das alterações. `commit` escreve páginas sujas e depois o cabeçalho limpo com a geração. O `.data` cresce por acréscimo de registros; slots de superbloco e páginas do índice são atualizados.

Salvar novamente um step acrescenta um novo registro e muda sua localização no índice. O registro anterior permanece no disco, mas deixa de ser a versão ativa desse step. Por isso geração e quantidade de steps são conceitos diferentes: uma substituição aumenta a geração sem aumentar a quantidade de chaves.

Não existe compactação de versões antigas. A sequência atual usa flush de streams, sem `fsync`/`FlushFileBuffers`, WAL ou lock de escritor entre processos; não é uma garantia ACID completa.

## 13. Abertura, leitura e recuperação

Uma abertura saudável confere superblocos, metadados e compatibilidade do índice. Reutiliza a raiz persistida e não varre todos os registros. Os nós e registros acessados são validados durante a leitura; abrir rapidamente não certifica todos os payloads históricos.

Se o índice estiver ausente, dirty ou incompatível, ou o tamanho físico exigir recuperação, o core percorre cabeçalhos, diretórios e rodapés. Os payloads são pulados durante essa reconstrução de metadados. As versões mais recentes de steps repetidos voltam a ser as ativas.

Uma cauda incompleta preserva o prefixo válido e bloqueia novas gravações. Não é truncada automaticamente. `repair-tail` é a operação explícita para descartar somente a cauda incompleta identificada. Corrupção na parte confirmada gera erro, em vez de descarte silencioso. Se um append falhar, a instância deve ser descartada e o dataset reaberto.

As leituras têm alcances distintos:

| Operação | O que carrega |
| --- | --- |
| `get_checkpoint` | Descritor, campos declarados e métricas |
| `range_steps` | Descritores de um intervalo inclusivo |
| `load_field` | Valores de um campo nomeado |
| `load_region` | Amostras de uma região, com verificação por blocos |
| `load` | Campos do checkpoint, dentro do orçamento agregado |
| `validate quick` | Estrutura do índice e descritores ativos |
| `validate full` | Todos os registros confirmados e seus payloads, inclusive versões substituídas |

A carga padrão é limitada a 256 MiB. `range_steps` aceita até 100000 descritores e orçamento agregado aproximado de 16 MiB. Esses limites exigem paginação ou seleção de dados; não representam limite total de tamanho do dataset.

Um mutex serializa métodos de uma instância DatasetStore. O chamador deve garantir escritor exclusivo entre instâncias/processos. Mesmo aberturas para consulta exigem atualmente permissão de escrita, pois podem reconstruir o índice.

## 14. Leitura regional e cache .nvb

Uma região é definida por três vetores: `origin`, `count` e `stride`. Eles têm o mesmo rank do campo:

```text
ultimo_indice_no_eixo = origin + (count - 1) * stride
```

`count` é a quantidade de amostras de saída. `stride` é a distância entre amostras na malha original. Um corte Z de uma grade 48³ pode usar origem `(0,0,24)`, count `(48,48,1)` e stride `(1,1,1)`.

O NAVIER02 original guarda CRC de campo inteiro. Para verificar regiões, a primeira leitura de um campo/registro percorre todo seu payload em blocos de 64 KiB, confere o CRC original e constrói uma tabela derivada de CRCs por bloco. Esse percurso não aloca o campo inteiro em RAM.

O cache fica em `.data.nvb/`. Cada tabela `.nvc` tem cabeçalho de 128 bytes, assinatura `NVBLOCK1`, identidade, geração do registro, localização e CRC original, além de CRCs do cabeçalho e da tabela. Depois vêm os CRC32 de cada bloco. A publicação usa arquivo temporário e rename; leitores concorrentes só aceitam um resultado existente idêntico. Cache inválido é reconstruído após nova validação integral do campo.

Com cache pronto, a leitura acessa e verifica somente os blocos necessários. Uma fatia Z contígua tende a tocar poucos blocos; uma fatia X ou amostragem dispersa pode tocar todos. Menor saída HTTP não garante menor leitura do `.data` em toda orientação.

O orçamento limita a saída; há também buffer de 64 KiB, metadados e tabela de CRC limitada a 16 MiB. A tabela permite campo original de até 256 GiB nesta implementação. Isso não equivale a teste de um campo real desse tamanho. Cache e arquivos temporários antigos não têm coleta automática.

Após construir o cache, a consulta regional certifica os blocos acessados. Detectar corrupção em outras regiões continua sendo tarefa de `validate full`. A primeira construção pode ser lenta e exceder o timeout HTTP de 60 segundos; campos grandes podem ser preparados pela CLI antes da GUI.

## 15. Consultas por métricas

`QueryEngine::build_metric_index(name)` percorre metadados e constrói uma B+ tree em RAM com chave `(valor, step)`. O step desempata valores iguais, preservando todos os checkpoints.

Depois, uma consulta pode localizar estados cujo diagnóstico esteja dentro de um intervalo. Os padrões são até 100000 entradas e 8 índices. Uma mudança de geração torna o índice antigo inválido e exige reconstrução.

Esses índices não estão no `.nvi`, não sobrevivem ao encerramento e não indexam cada célula de um volume. A CLI constrói o índice de métrica naquela execução. Índices espaciais e consultas compostas continuam pendentes.

## 16. O visualizador

A GUI usa React e TypeScript para interface/estado, workers para parte da leitura e preparação, e Three.js/WebGL2 para renderização. O catálogo local NAVIER02 chama a CLI real por um transporte HTTP do Vite, com fila, cancelamento e limites de resposta. Não existe ainda um daemon C++ permanente.

A timeline seleciona um checkpoint; a camada de dados busca o campo e o engine prepara sua representação. Há cache de dados no viewer, amostragem conforme qualidade e controle de recursos GPU. LOD visual é uma aproximação da exibição, não um novo resultado do solver.

São oferecidos mapas 2D, cortes X/Y/Z, volume por ray marching, pontos, glifos, linhas de fluxo RK2 e isosuperfícies por marching tetrahedra. O orçamento de triângulos da isosuperfície tem redução automática de resolução. A câmera permite orbitar, mover e aproximar; a timeline permite animar estados já gravados.

Cortes de campos nativos e módulos vetoriais usam leitura regional, com posicionamento e inspector na malha original. Rotacional exige vizinhança e ainda usa campo completo; sobreposições vetoriais, volume e isosuperfície também permanecem no caminho integral. Respostas de campo/região da CLI usada pelo viewer são limitadas a 64 MiB.

Há colormaps, escalas, comparação visual e exportação PNG. Comparação ainda não calcula diferenças numéricas entre checkpoints; inspector preciso está disponível nos planos. Estatísticas da representação amostrada não devem ser confundidas com uma análise integral do volume original.

## 17. Os exemplos e a física

Os produtores demonstram integração e produzem arquivos reais. Incluem calor 2D, fluidos por LBM, cavidade incompressível, ondas 3D, reação-difusão Gray–Scott, Lorenz e eletromagnetismo.

Os exemplos recentes de antena usam a solução analítica retardada de dipolos ideais. A interferência soma os campos E e B das duas fontes antes de calcular energia e fluxo, incluindo os termos cruzados. Fase e separação são parâmetros de geração. Não há modelagem de fios metálicos ou acoplamento entre antenas reais.

A máscara das fontes exclui regiões singulares; NaNs nessas células são intencionais. Linhas e isosuperfícies ajudam a interpretar os campos, mas não substituem estudos de convergência ou validação científica independente.

Os detalhes dos métodos e limites estão em [simulações](simulations.md), [antena](antenna3d.md) e [interferência](interference3d.md).

## 18. Como usar na prática

Na instalação Windows já compilada, abra a GUI:

```powershell
cd F:\Users\EHD\Github\er\Navier
.\start_viewer.cmd
```

O launcher instala dependências quando necessário, compila o frontend e inicia o servidor. Abra o endereço informado no terminal, normalmente `http://127.0.0.1:4173`, e selecione **Exemplos locais**. Mantenha o terminal aberto; Ctrl+C encerra o servidor. Node.js é necessário.

Para produzir outra execução das duas antenas, use um nome novo:

```powershell
.\build-msvc\Release\navier_interference3d.exe interferencia_nova.data 48 600 6 90 4
```

Os argumentos representam arquivo, lado da grade, passos, intervalo de gravação, fase em graus e separação. Gerar uma nova simulação e reproduzir um arquivo existente são operações diferentes: a reprodução não recalcula a física.

O core também pode ser usado sem GUI:

```powershell
.\build-msvc\Release\navier.exe info interferencia_nova.data
.\build-msvc\Release\navier.exe validate interferencia_nova.data full
.\build-msvc\Release\navier.exe region-info-json interferencia_nova.data 480 electric_z 0,0,24 48,48,1 1,1,1
```

Para compilar C++ na toolchain MSVC documentada:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Release --parallel 1
ctest --test-dir build-msvc -C Release --output-on-failure
```

O projeto requer CMake 3.24+ e compilador com suporte ao C++23 usado pelo código. O gerador deve corresponder à instalação local. Testes usam GoogleTest; se ele não estiver instalado, a configuração pode precisar de rede para baixá-lo.

## 19. O que foi medido e testado

Um dataset real de **10759016032 bytes**, aproximadamente 10 GiB, com **2560 checkpoints**, passou por validação completa. Seu índice ocupou **131072 bytes**, ou 128 KiB. Isso ilustra que o índice cresce com a quantidade de checkpoints, enquanto o `.data` cresce com seus campos.

Nesse ensaio, a abertura saudável não varreu registros. Os campos individuais tinham alguns MiB: o teste não significa leitura de um único campo de 10 GiB. Tempos e throughput dependem do hardware e de caches do sistema, que não foram controlados.

Na medição regional desse dataset, um campo de 2105344 bytes produziu uma região de 1024 bytes. Com cache pronto, a chamada leu 65779 bytes do `.data`, cerca de 32 vezes menos que o payload completo. A primeira chamada leu 2171123 bytes para construir e validar o cache.

No exemplo de interferência 48³, um corte devolveu **9216 bytes**, contra **442368 bytes** do campo inteiro: redução de 48 vezes na resposta. Esses contadores são bytes lógicos solicitados ao arquivo e excluem índice/cache; não medem bytes físicos de disco nem demonstram a mesma redução de latência.

A etapa de leitura regional foi validada com **107 testes C++ em Release e Debug**, **37 testes do viewer** e build de produção. Os testes cobrem splits, reabertura, intervalos, duplicatas, CRCs, truncamento, regiões, limites, caches e posicionamento dos cortes. Testes dos produtores cobrem propriedades específicas de cada método.

Há comparação com outras estruturas de dados em RAM, mas não comparação implementada que demonstre superioridade sobre HDF5/NetCDF. Resultados e condições de medição: [benchmarks](benchmarking.md) e [leitura regional](regional-io.md).

## 20. Onde está cada parte no código

| Caminho a partir da raiz | Responsabilidade |
| --- | --- |
| `include/navier/index/bplus_tree.hpp` | B+ tree genérica em RAM |
| `include/navier/index/persistent_bplus_tree.hpp` | Contrato da árvore persistente |
| `src/storage/persistent_bplus_tree.cpp` | Páginas de nós, busca, split e validação |
| `src/storage/checkpoint_store.cpp` | NAVIER01 e reconstrução do índice legado |
| `include/navier/storage/field.hpp` | Campos, métricas, descritores e regiões |
| `src/storage/dataset_store.cpp` | NAVIER02, save/load e recuperação |
| `src/storage/field_region.hpp` | Leitura regional e cache de CRCs |
| `src/storage/page_manager.cpp` | IO de páginas do índice |
| `src/storage/buffer_pool.cpp` | Cache LRU, pins e páginas dirty |
| `include/navier/format/binary.hpp` | Serialização explícita, CRC e aritmética verificada |
| `include/navier/query/query_engine.hpp` | Índices secundários de métricas em RAM |
| `tools/navier_cli.cpp`, `tools/dataset_cli.hpp` | Interface de terminal |
| `examples/` | Produtores e ferramentas de exemplo |
| `viewer/src/data/` | Adapters e cache do viewer |
| `viewer/src/visualization/` | Engine, representações e geometria |
| `viewer/vite.config.ts` | Transporte local para o core |
| `tests/`, `viewer/tests/` | Verificação nativa e frontend |

## 21. Valor atual e próximos passos

O valor atual é permitir que um produtor C++ salve um histórico científico descrito e que outras ferramentas consultem esse histórico sem recalcular a física ou carregar todo o dataset. O projeto também torna explícitas as decisões de formato, indexação, integridade e memória, servindo como base de estudo e experimentação de sistemas.

Já existe uso real em exemplos locais, arquivos grandes e exploração visual. A maturidade ainda é experimental. As próximas frentes são:

1. **Durabilidade e concorrência:** sincronização com o armazenamento, lock de escritor, protocolo de recuperação/transação definido e testes de interrupção abrupta.
2. **IO científico:** chunks/bricks nativos, compressão, halos para derivadas e multirresolução persistida.
3. **Consultas:** índices de métricas persistentes, consultas compostas, timeline completamente paginada e serviço C++ permanente.
4. **Manutenção:** compactação de versões, migração explícita e comparações controladas com formatos científicos maduros.
5. **Análise e física:** diferenças numéricas entre estados, seleção volumétrica, exportação científica, retomada completa e estudos de convergência.

Esses recursos não estão implícitos na existência de uma B+ tree ou de um arquivo binário. Cada um exige implementação e verificação próprias. O [roadmap](roadmap.md) acompanha os limites atuais.

## 22. Leituras complementares

- [Arquitetura](architecture.md)
- [Formato binário](file-format.md)
- [B+ tree em RAM](bplus-tree.md)
- [Índice persistente](persistent-index.md)
- [Armazenamento por campos](checkpoint-storage.md)
- [Recuperação](recovery.md)
- [Consultas](query-engine.md)
- [Leitura regional](regional-io.md)
- [Integração core/viewer](viewer.md)
- [Instruções da GUI](../viewer/README.md)

