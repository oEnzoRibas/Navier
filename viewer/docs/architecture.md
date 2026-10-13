# Arquitetura e separação

A UI recebe NavierDataSource; não calcula offsets e não conhece C++ nem a serialização. LocalNavierDataSource implementa o formato atual usando File.slice ou HTTP Range. InMemoryDataSource é uma implementação pequena para testes e dados já residentes, não um substituto inventado para o core.

Fluxo: arquivo → worker de parsing → diretório de descritores → campo solicitado → cache limitado → worker de análise → representação float32 → GPU. React guarda descritores, configurações e estatísticas pequenas. Arrays científicos e objetos Three.js ficam nos adapters e engines, fora do estado React. O contrato admite Float32Array ou Float64Array científicos; renderValues é opcional. Um futuro adapter pode devolver somente o campo e a camada visual prepara float32 em worker.

App coordena datasets e preferências. Cada viewport possui engine, câmera e worker de geometria. O adapter local possui seu worker de leitura/análise e LRU. Ao fechar dataset o worker é terminado e o cache liberado; ao desmontar viewport controles, observer, RAF, texturas, materiais, geometrias e worker são descartados; a vida do contexto pertence ao canvas (não é forçada a perda em hot reload). Trocar checkpoint reutiliza câmera e contexto mas substitui os recursos do campo.

O diretório dos checkpoints é linear em quantidade de registros, não em payload. Não contém um B+ Tree JS. NavierCoreDataSource já usa o core C++ para metadados, campos e índice temporal persistente. A interface atual lista descritores completos: paginação por cursor será necessária para dezenas de milhões de registros, mas a timeline usa um único slider, sem um elemento DOM por checkpoint.

Cancelamento usa AbortSignal e IDs de tarefas: resposta obsoleta não atualiza a viewport. Leituras HTTP são abortáveis; File.slice em andamento não pode ser fisicamente interrompido, mas seu resultado é rejeitado. Loops CPU limitados são síncronos no worker; um cancelamento pode aguardar o término desse loop. Ao fechar, terminate interrompe o worker.

Sem consulta científica falsa, métricas aleatórias, renderização baseada em nomes de solvers ou dados simulados na interface. O catálogo Vite lista arquivos reais: ranges para o legado, proxy CLI C++ para NAVIER02.

A preparação usa FramePipeline: mantém apenas atual/próximo e LRU de 64 MiB por viewport, deduplica tarefas concorrentes e promove prefetch sem cancelá-lo ao avançar. Saltos cancelam jobs obsoletos. Ajustes visuais ficam fora da chave de preparação. Durante IO o estado anterior continua visível e a câmera permanece interativa; indicação discreta só aparece depois de 400 ms. Reprodução mira 12 checkpoints/s em 1×, ajustável, aguardando preparação quando necessário.
