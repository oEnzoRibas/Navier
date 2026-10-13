# ADR: Páginas e pool limitado

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Persistência requer identidade estável e RAM independente do arquivo.

## Decisão

PageId u64, páginas 4096 e LRU com pin/dirty via RAII.

## Alternativas

Índice inteiro RAM, mmap e cache do SO apenas.

## Consequências

Controle e métricas explícitos, custo de serialização e pin correto.
