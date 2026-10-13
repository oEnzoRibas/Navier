# Páginas e buffer pool

PageManager aloca/lê/escreve páginas fixas de 4096 bytes. Arquivo parcialmente paginado é rejeitado; offsets são verificados. BufferPool mantém limite de páginas (64 por padrão) e política LRU. A consulta traz página ausente; descarte escreve página dirty antes de removê-la.

Guard move-only mantém pin e libera por RAII. Páginas pinned não são descartadas. Esgotar todas as páginas pinned é erro explícito. mark_dirty registra alteração. flush rejeita página dirty ainda pinned; commit exige liberação dos guards. Guard não pode sobreviver ao pool. Destrutor tenta flush e suprime erros; chame flush/commit explicitamente para observar falhas.

Estatísticas: hits, misses, evictions, dirty writes, páginas cached/pinned. O pool é limitado independentemente do tamanho do arquivo. Não é thread-safe isoladamente; DatasetStore serializa acesso. flush é de stream, sem promessa de fsync/FlushFileBuffers.
