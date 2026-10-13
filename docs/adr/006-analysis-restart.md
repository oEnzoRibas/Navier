# ADR: Analysis versus restart

Status: aceito nesta implementação. Data: 2026-10-12.

## Contexto

Campos visuais nem sempre bastam para reconstruir solver.

## Decisão

Kind explícito no registro; novos produtores declaram analysis.

## Alternativas

Chamar todo checkpoint restart sem verificar estado.

## Consequências

Rotulagem honesta; retomada ainda responsabilidade do produtor.
