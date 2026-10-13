# ADR: Campos tipados selecionáveis

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Vector<double> monolítico não descreve variáveis/precisão/formas.

## Decisão

NAVIER02 com diretório por checkpoint e f32/f64, dims, unidade, CRC.

## Alternativas

Alterar NAVIER01; depender de sidecars; HDF5.

## Consequências

Legado preservado; dois formatos; sem chunks nesta fase.
