# Arquivos grandes

Abertura lê 8 bytes de assinatura e 24 por registro, pula payloads por offset e constrói descritores. Nenhum File.arrayBuffer do arquivo completo, base64 dataset no HTML ou todos os campos/estados em RAM. Local File usa [Blob.slice](https://developer.mozilla.org/en-US/docs/Web/API/Blob/slice). HTTP exige status 206 e Content-Range exato antes de ler o body; servidor que responde 200 é rejeitado e seu body cancelado.

Um pedido lê somente o plano do campo escolhido. Qualidade padrão limita a 2 milhões de amostras; opções 250 mil/8 milhões. Em LOD percorre apenas linhas Y/Z selecionadas e amostra X com stride; lê a linha inteira para evitar milhões de IOs pequenos. Um campo acima de 192 MiB de buffers finais é rejeitado. O volume impõe ainda limite de textura 3D/GPU, máximo 256 voxels por eixo no carregamento para ray marching. O limite de cache global é configurável 64–512 MiB (128 padrão), dividido entre os datasets. Dados em uso e transitórios ficam fora do orçamento LRU.

Global range é uma operação solicitada pelo usuário, sequencial e cancelável. Extremos/médias de LOD referem-se às amostras, não são garantidos para todo o arquivo. Série de estados visitados é limitada a 512; metadados podem fornecer séries existentes sem qualquer varredura de campos.

## O que ainda falta para 10 GB práticos

O parser foi testado com endereçamento virtual de 10 GiB lendo apenas 32 bytes de cabeçalhos, e com dataset real de 3.606 MiB. Isso não é benchmark de uma execução completa com 10 GiB. Ainda é necessária avaliação real em hardware/browser diferentes, tempo de startup O(registros), memória transitória e IO de LOD por linhas. 100 mil descritores foram testados na timeline/busca, não 100 mil ranges HTTP no navegador.

Para escalar throughput: diretório persistente no core; cabeçalhos agrupados, reads regionalizados por bricks, LOD persistido/extremos por brick, cache separado de dados/representações, scheduler limitado de jobs e deduplicação; cancelamento cooperativo de CPU; paginação de diretório e métricas indexadas. Um campo enorme pode precisar de estratégia de regiões e não apenas reduzir samples. Sparse real exige serialização própria. O viewer atual não promete carregar/renderizar todo um dataset de 10 GB de uma vez.
