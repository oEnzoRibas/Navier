# B+ Tree em RAM

A árvore genérica foi escrita em C++, sem std::map como implementação. Order limita filhos; Order=32 comporta 31 chaves. Dados ficam nas folhas, ligadas para consultas inclusivas ordenadas. Separadores internos representam mínimos dos filhos à direita. Busca usa descida por separadores; overflow divide folha/interno e pode criar nova raiz. Duplicata substitui valor, sem aumentar size. clear cria nova raiz vazia antes de substituir a anterior.

validate verifica ordenação, ocupação, separadores, profundidade, contagem e encadeamento. Há oráculos com std::map nos testes, inclusive 100000 operações mistas determinísticas; não se trata de implementação delegada a map.

Busca/inserção: O(log N); intervalo: O(log N + K). Hash pode ser melhor para igualdade; vetor ordenado pode ser melhor para dados estáticos. Steps perfeitamente regulares e registros de tamanho fixo podem usar endereço calculado sem árvore. A B+ Tree é útil para steps irregulares, substituições e intervalos, especialmente páginas persistentes. Resultados reais em [benchmarking](benchmarking.md).
