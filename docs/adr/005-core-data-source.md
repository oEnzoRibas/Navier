# ADR: Fronteira do viewer

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Reimplementar índice no browser duplica regras e impede grandes arquivos.

## Decisão

Adapter solicita CLI C++ real por proxy local limitado.

## Alternativas

Parser JS do formato novo; serviço permanente C++ imediatamente.

## Consequências

Reutiliza core; custo por processo e limites de transporte; serviço permanente futuro.
