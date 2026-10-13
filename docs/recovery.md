# Recuperação e integridade

Ordem de escrita: registro e NVCOMMIT -> flush do .data -> atualização dirty/inserção/commit do índice -> slot alternado do superbloco. Cabeçalhos, diretório, metadados, campos e páginas têm checksums separados.

Dois slots permitem recuperar um cabeçalho recente danificado. Mudança de tamanho físico, índice ausente/dirty ou identidade/geração divergente inicia varredura de cabeçalhos/diretórios/rodapés. Payloads são pulados. Registros completos são recuperados; step duplicado aponta para último registro. Corrupção na região confirmada é erro, não descarte silencioso.

Cauda incompleta preserva prefixo válido e não é truncada automaticamente. Escrita fica bloqueada. `navier repair-tail arquivo.data` descarta explicitamente somente a cauda incompleta identificada e atualiza o superbloco. Faça backup antes de uma reparação que você decidiu executar. Falha durante append inutiliza a instância: descarte e reabra.

`validate quick` confere estrutura do índice e descritores vivos. `validate full` percorre todos os registros confirmados, incluindo versões substituídas, verificando payloads em blocos de 64 KiB. Cauda incompleta preservada é sinalizada por info; não é certificada pelo validate do prefixo.

Testes simulam truncamento em todos os limites de um registro, páginas dirty, ausência de índice e corrupção de diretórios/payloads. Isso não equivale a teste de queda de energia. Ausência de fsync, WAL e locks de processo limita durabilidade/concorrência; não há ACID completo.
