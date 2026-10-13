# ADR: Registros append-only

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Substituir campos in-place dificulta offsets e recuperação.

## Decisão

Acrescentar registros e atualizar referência temporal para última versão.

## Alternativas

Reescrita do arquivo, slots fixos e armazenamento por campo externo.

## Consequências

Histórico verificável e escrita simples; espaço cresce, compactação futura.
