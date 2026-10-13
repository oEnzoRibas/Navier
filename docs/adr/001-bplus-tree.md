# ADR: Índice temporal B+ Tree

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Steps irregulares e ranges exigem ordem, com atualizações incrementais.

## Decisão

Manter árvore própria RAM e implementação paginada concreta.

## Alternativas

Hash favorece igualdade; vetor ordenado favorece dados estáticos; endereço calculado serve registros periódicos fixos.

## Consequências

Mais complexidade e testes estruturais; não é sempre a estrutura mais rápida.
