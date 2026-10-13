# Índice temporal persistente

PersistentBPlusTree mapeia uint64 step para (offset,length). Folhas: floor((4092-32)/24)=169 entradas. Internos: floor((4092-32-8)/16)=253 separadores, 254 filhos. Ocupação mínima fora da raiz: 85 entradas nas folhas e 126 chaves internas. Overflow divide incrementalmente; não reescreve toda a árvore por inserção. Leaves mantêm próximo PageId.

Busca passa por páginas até folha. Range percorre folhas encadeadas. Substituição conserva quantidade. A raiz e geração sobrevivem à reabertura. Nós são decodificados com bounds/CRC; PageIds, ciclos, links, profundidade e ocupação são validados. Limite de descida 64 evita laços em arquivos corrompidos.

begin_update persiste marcador dirty antes das mudanças. commit escreve páginas sujas e depois cabeçalho limpo. DatasetStore associa o índice à identidade e geração do .data. Índice inconsistente é cache derivado reconstruível: varre registros completos, constrói .nvi.rebuild e substitui .nvi. Não há WAL nem isolamento transacional entre processos; veja [recovery](recovery.md).

Teste: 50000 chaves, níveis múltiplos, inserção aleatória, substituições, intervalos, reabertura com pool de 3/7 páginas. Arquivo real de 10 GiB usou índice de 128 KiB (32 páginas), para 2560 checkpoints: tamanho depende da quantidade de checkpoints, não dos bytes de campos.
