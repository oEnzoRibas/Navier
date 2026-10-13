# Formatos binários

Todos os inteiros são little-endian; floats são bits IEEE 754. Nenhum ponteiro, padding de struct ou objeto STL é serializado. Operações aritméticas de tamanho/offset são verificadas antes de alocar. CRC32 usa polinômio IEEE 0xedb88320; detecta corrupção acidental, não autentica dados.

## NAVIER01: compatibilidade

Formato legado: cabeçalho e registros step/time/vector<double>. Não foi alterado. CheckpointStore reconstrói índice RAM na abertura e não oferece checksums ou recuperação do formato novo. Dados e sidecar JSON continuam utilizáveis pelo viewer. Não existe migração automática.

## NAVIER02: superbloco

Região inicial de 4096 bytes. Dois slots de 128 bytes nos offsets 0 e 256; escolhe-se o slot válido mais recente. Metadados opacos (convencionalmente JSON) começam em 4096, até 1 MiB. O início de registros é alinhado em 8 bytes.

| Offset no slot | Tipo | Conteúdo |
|---|---|---|
|0|8 bytes|NAVIER02|
|8|u32|versão 2|
|12|u32|marcador 0x01020304|
|16,20|u32,u32|página 4096, flags 0|
|24,32,40|u64|geração, fim confirmado, início dos registros|
|48,56|u64|offset e tamanho de metadados|
|64|u32|CRC de metadados|
|72,80|u64|identidade do dataset (par)|
|88,96|u64|raiz temporal espelhada, quantidade de steps vivos|
|104,112|u64|primeiro e último step|
|124|u32|CRC dos primeiros 124 bytes|

Bytes reservados permanecem zero. O espelho da raiz não substitui a identidade/geração do índice.

## Registro de checkpoint

Cabeçalho de 64 bytes seguido de diretório, payloads e rodapé de 16 bytes.

| Offset | Tipo | Conteúdo |
|---|---|---|
|0|8 bytes|NVCPT002|
|8,16|u64|comprimento total, step|
|24|f64|tempo finito|
|32|u64|geração sequencial|
|40,44,48|u32|quantidade de campos, métricas, bytes do diretório|
|52|u32|0 analysis / 1 restart|
|56,60|u32|CRC do diretório, CRC dos primeiros 60 bytes|

Limites: 1–1024 campos, 0–1024 métricas, diretório até 1 MiB. Cada campo: nome e unidade (u16 comprimento + até 255 bytes cada), tipo u8 (1 float32 / 2 float64), rank u8 (1–8), codec u16 (0 raw), dimensões u64 positivas, offset relativo u64, bytes armazenados u64, bytes brutos u64, CRC u32. Produto das dimensões e bytes devem coincidir; campos são contíguos. X varia mais rápido.

Cada métrica contém nome, unidade e f64 finito. O rodapé contém NVCOMMIT (8 bytes) e comprimento total u64. Payloads mantêm precisão f32/f64. Codec e tamanho bruto preparam extensão futura, mas compressão e chunks não existem hoje.

## Índice .nvi

Páginas de 4096 bytes. Página 0: NVINDEX1, versão u32 em 8, pageSize u32 em 12; root/size/generation/dirty u64 em 16/24/32/40; identidade do dataset em 48/56. CRC em 4092 cobre os primeiros 4092 bytes.

Nós: BPT1 em 0, tipo u8 em 4 (1 folha/0 interno), count u16 em 6, próximo PageId de folha u64 em 8, entradas em 32, CRC em 4092. Folhas armazenam (step,offset,length), 24 bytes. Internos armazenam child0 e pares (separator,child). PageId identifica páginas, nunca endereço de memória. Consulte [índice](persistent-index.md).
