# ADR: IO seletivo com limites

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Arquivo grande não deve implicar grande alocação.

## Decisão

Descritores sem payload; load_field limitado; blocos 64 KiB e checksums separados.

## Alternativas

Load integral obrigatório; cache ilimitado.

## Consequências

IO menor e previsível; campo ainda monolítico, regiões futuras.
