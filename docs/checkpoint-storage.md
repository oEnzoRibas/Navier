# Armazenamento por campos

DatasetStore::save recebe spans emprestados de FieldView e métricas. O produtor mantém os buffers vivos até a chamada retornar. O core calcula checksums e serializa em blocos de 64 KiB. Há duas passagens sobre campos (checksum e escrita), sem cópia monolítica do checkpoint.

get_checkpoint/range_steps leem diretórios e métricas. load_field lê somente o campo nomeado; load_region lê amostras de uma região com verificação por bloco e cache derivado (primeiro acesso valida o campo inteiro); load aplica orçamento agregado antes de reunir todos os campos. Orçamento padrão: 256 MiB por carga. range_steps aceita no máximo 100000 descritores e orçamento agregado aproximado de 16 MiB; diminua a página se exceder.

Step duplicado acrescenta novo registro e substitui a localização no índice. A versão anterior permanece no arquivo; validação completa cobre também essa história. Não há compactação nem exclusão. Dimensões/tipos podem mudar por checkpoint.

Analysis significa dados suficientes para inspeção. Restart é uma declaração do produtor, não uma garantia automática: estados internos, condições e parâmetros necessários ao reinício devem ser salvos. Os novos exemplos salvam analysis; não oferecem retomada exata.

Criação recusa substituir um .data existente. Abertura saudável usa superbloco e índice persistente, sem varrer registros. Ausência/inconsistência do índice dispara reconstrução de metadados, sem carregar campos. Mesmo operações lógicas de leitura podem reconstruir .nvi; abertura atualmente exige permissões de escrita.
