# Consultas

DatasetStore: busca exata por step, intervalo inclusivo ordenado, descritores de campos e métricas, leitura de um campo ou checkpoint limitado. Não é preciso carregar campos para navegar na timeline.

QueryEngine::build_metric_index(name) constrói explicitamente B+ Tree RAM com chave (valor,step), preservando valores iguais. Limites padrão: 100000 entradas e 8 índices. query_metric exige índice construído e bounds finitos, com até 100000 resultados. Geração é verificada; save invalida a consulta até reconstrução. A construção varre somente metadados.

CLI `query arquivo.data max_v 0.1 0.4` constrói índice RAM naquela execução. Não é índice persistente nem consulta gratuita. ROI, consultas espaciais, expressões e análises multidataset estão pendentes. Não é anunciado um índice de valor sobre cada célula.
