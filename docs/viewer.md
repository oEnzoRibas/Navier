# Viewer conectado ao core

Execute start_viewer.cmd na raiz, após compilar Release. Em Exemplos locais, NAVIER02 é identificado pelo magic e usa a CLI C++ real. Não requer sidecar. NAVIER01 usa parser legado e .data.json. Upload local continua destinado ao legado; o novo formato usa catálogo/proxy local.

Vite restringe caminhos ao catálogo do projeto. IDs derivam de hash do caminho relativo. Endpoints GET info/checkpoint/list/field chamam CLI com argumentos separados, sem shell. Duas solicitações simultâneas, 16 em espera, timeout de 60 s; cancelamento encerra processo. Respostas JSON até 16 MiB e binário até 64 MiB. Não há API de escrita exposta.

NavierCoreDataSource abre metadados em páginas, monta métricas salvas e solicita apenas o campo selecionado. Limites: diretório de timeline aproximadamente 64 MiB e 1 milhão de entries; timeline plenamente baseada em cursores é futura. Viewer suporta rank 1–3; core suporta 1–8. Extração é do campo inteiro: corte/LOD não são region IO.

Workers decodificam f32/f64, calculam derivadas e geometria, respeitando cache de 128 MiB e orçamentos de render. Precisão científica fica separada de Float32 GPU. Prefetch, reutilização de frames e troca de aparência evitam recarga contínua; instrumentação exibe IO/cache/FPS. Renderização volumétrica usa ray marching. Isosuperfície usa marching tetrahedra real (6 tetraedros/célula), interpolação e normais, malha Three.js, máximo 100000 triângulos. Processa camadas com cancelamento; não confundir isso com chunks persistentes.

Rótulos de frentes de onda anteriores ainda usam pontos filtrados; selecione Isosuperfície para malha real. Gradientes, limites globais e streamlines da interface podem usar LOD; não substituem análise numérica validada. API permanente, streaming por bricks e consultas regionais estão no roadmap.
