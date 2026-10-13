# Navier Viewer

Aplicativo científico local React + TypeScript estrito + Three.js/WebGL2. Abre os checkpoints existentes sem embutir payloads no HTML. O core e os solvers C++ continuam independentes do frontend.

## Executar

Requer Node.js 20.19+ ou 22.12+; verificado com Node 24.12.0. No diretório do projeto:

```powershell
cd F:\Users\EHD\Github\er\Navier
.\start_viewer.cmd
```

O launcher instala dependências quando necessário, compila e inicia o viewer em http://127.0.0.1:4173. Se a porta estiver ocupada, o servidor informa o erro: use outra porta explicitamente com `npm run preview -- --port 4174`. Encerre com Ctrl+C.

Fluxo manual:

```powershell
cd viewer
npm ci
npm run build
npm run preview
# desenvolvimento: npm run dev
# verificações: npm test; npm run typecheck
```

Abra **Exemplos locais** e escolha um arquivo existente, ou **Abrir arquivos** e selecione juntos `exemplo.data` e `exemplo.data.json`. Arrastar os dois arquivos também funciona. Os arquivos permanecem locais; o aplicativo não tem serviço em nuvem. O catálogo usa HTTP Range para NAVIER01 e uma API local que executa a CLI C++ real para NAVIER02. O novo formato não requer sidecar e é aberto pelo catálogo após compilar Release. Uma hospedagem estática pode usar seleção local de arquivos sem esse catálogo.

Escolha campo, representação e qualidade à direita. Espaço reproduz/pausa; setas navegam, Shift+seta avança dez estados. O salto textual aceita steps uint64 sem perda de precisão. Em 3D arraste para orbitar, botão direito para mover, roda para zoom; R/F enquadram. Em 2D o arraste move o plano. O inspector numérico funciona sobre heatmaps/cortes. PNG exporta a viewport principal pronta; comparação mostra dois checkpoints lado a lado. Métricas não exigem carregar todos os estados.

## Modos e alcance

Implementado: campo escalar 2D, cortes X/Y/Z, nuvem GPU, volume 3D por ray marching, glifos vetoriais, linhas de fluxo RK2, pontos com limiar para frentes de onda e células ocupadas; colormaps genérico/sequencial/divergente/térmico/viridis/cinza, escalas auto/fixa/manual/global explícita, simetria/log positivo, thresholds e opacidade; timeline, prefetch, comparação visual, PNG, temas, cache e diagnóstico GPU.

Implementado também: malhas de isosuperfície por marching tetrahedra com isovalor, normais e cancelamento.

Parcial: frentes de onda são pontos filtrados, sem extração de superfície; células ocupadas são pontos de uma grade densa amostrada, não um formato sparse; comparação não calcula diferenças; inspector apenas em planos; global e estatísticas usam a representação LOD selecionada; streamlines limitadas com interpolação simples, não integração científica de alta ordem.

Futuro: streaming por bricks e regiões, LOD persistido, consulta espacial/valor/ROI, índices de métricas persistentes, comparação numérica, seleção 3D volumétrica precisa. Nenhum backend fictício ou B+ Tree JavaScript foi criado.

## Dependências

React/ReactDOM: UI e estado pequeno. TypeScript: contrato estrito entre camadas. Three.js: recursos WebGL2, câmeras, OrbitControls e linhas com espessura. Vite/plugin React: build e workers empacotados. Vitest: testes de leitura, cache, análise e timeline. Dependências e workers são locais no bundle; nenhum CDN de renderização.

## Organização

- `src/data`: contrato NavierDataSource, adapters local/in-memory, ByteLRU e ownership de buffers.
- `src/parsers`: NAVIER01, metadados, ranges e workers.
- `src/analysis`: campos derivados e geometria científica.
- `src/visualization`: engine Three.js, colormaps, planos, vetores, volumes e worker de geometria.
- `src/components`, `app`, `state`, `styles`: interface, timeline e preferências leves.
- `legacy`: viewer original preservado, incluindo as alterações anteriores de Maxwell, ondas e Lorenz. O exporter C++ continua usando essa versão.

[Arquitetura](docs/architecture.md) · [DataSource](docs/data-source.md) · [Renderização](docs/rendering.md) · [Arquivos grandes](docs/large-datasets.md) · [Verificação](docs/verification.md).

Detalhes e limites atuais da integração: [core/viewer](../docs/viewer.md).
