# Antena oscilante 3D

Dipolo elétrico ideal prescrito, orientado em Z, em espaço livre. Avalia a solução analítica retardada de Maxwell e amostra os campos numa malha. Não é uma antena metálica de tamanho finito, cálculo de impedância ou solver FDTD. Referência: [Feynman II, seção 21-4, equações 21.23, 21.25–21.27](https://www.feynmanlectures.caltech.edu/II_21.html#Ch21-S4).

## Executar e repetir

Na raiz do projeto, após o build Release:

```powershell
.\build-msvc\Release\navier_antenna3d.exe antenna_nova.data 48 600 6
.\start_viewer.cmd
```

Argumentos: `[arquivo] [lado] [passos] [intervalo]`. Use nome novo; recusa saída existente. Padrão: 48³, 600 passos com dt=.05, cada 6 e também o passo final. A solução é avaliada diretamente nos tempos salvos, sem integrar passos intermediários. Lado 16–96, sujeito a pelo menos 6 amostras por comprimento de onda; com os parâmetros padrão, use 25–96. Pelo menos 40 amostras temporais por período. São limites de amostragem, não CFL de integrador.

Exemplo já gerado: `runs/showcase/antenna3d.data`, **580960521 bytes**, 101 checkpoints, índice de 8192 bytes. `validate full` passou. Os arquivos binários ficam fora do Git.

## Visualizar

Em **Exemplos locais**, selecione `runs/showcase/antenna3d.data`.

- **Módulo · electric** + **Linhas de campo / fluxo**: linhas elétricas instantâneas. Experimente 160–240 sementes e 100–200 passos de integração.
- **Módulo · magnetic** + o mesmo modo: laços magnéticos em torno de Z. Deixe o campo vetorial em Automático ou magnetic.
- **radiation_energy** + Volume: contribuição de radiação. Escala manual 0–.002 e limiar baixo .0001 são um ponto de partida; ajuste conforme estado/câmera.
- **potential** + Corte ou Isosuperfície: potencial escalar na gauge Lorenz, inclusive sinais opostos.
- **Módulo · poynting** + linhas: fluxo de energia E×B; no campo próximo pode haver fluxo local de retorno.

Para colorir energia e sobrepor linhas de B/E, selecione electric/magnetic em **Vetores e linhas de campo → Campo vetorial**. Evita usar o primeiro vetor do arquivo quando há vários campos.

Clique Reproduzir. Arraste para orbitar, botão direito para mover e roda para zoom. Step 480 é t=24. Step 0 está em repouso; o campo chega depois aos pontos distantes. Linhas são curvas tangentes ao campo num instante, não trajetórias de partículas ou objetos materiais carregados pela onda.

## Modelo

Unidades normalizadas: c=ε=μ=1. Janela [-8,8]³, dx=16/(N−1). Ondas saem sem refletir porque a solução é avaliada diretamente; não há PML numérica nem cavidade.

p(t)=a(t) sin(ωt) ẑ, ω=2π/4. Antes de t=0, p e derivadas são zero. Durante t=0–4, a(s)=6s⁵−15s⁴+10s³, s=t/4; depois a=1. Valor e duas primeiras derivadas são contínuos.

Com n=r/r, tᵣ=t−r e K=1/(4π), p e suas derivadas abaixo são avaliados em tᵣ:

```text
E = K[(3n(n·p)−p)/r³ + (3n(n·p')−p')/r²
      + (n(n·p'')−p'')/r]
B = K[(p' × n)/r² + (p'' × n)/r]
phi = K[n·p/r² + n·p'/r]
A = K p'/r
S = E × B
u = (|E|² + |B|²)/2
```

`radiation_energy` usa apenas os termos 1/r: |E_rad|², pois |B_rad|=|E_rad| nessas unidades. Não é energia total. `potential` depende da gauge: E=−grad(phi)−∂A/∂t. Portanto phi constante não precisa ser perpendicular às linhas de E neste caso variável no tempo.

A fonte pontual é singular: dentro de r<.6, campos são **NaN** e `source_mask=0`. Fora, mask=1. O viewer ignora inválidos e termina linhas na máscara. Não resolve cargas/correntes/condutores nessa região nem atribui campos finitos fictícios ao interior.

## Campos e métricas

13 campos float32: electric_x/y/z, magnetic_x/y/z, poynting_x/y/z, potential, energy_density, radiation_energy, source_mask. Fórmulas avaliadas em double antes de converter para float32. Dimensões, dx, origem, método, parâmetros, gauge e exclusão estão nos metadados NAVIER02.

Métricas: dipole_moment, energy_in_window e radiation_energy_in_window. Energias são somas amostradas fora da máscara vezes dx³. Não são integrais exatas ou prova de conservação: a fonte fornece energia e ondas atravessam a janela. Checkpoints são analysis.

## Validação e limites

95 testes C++ passaram em Release e Debug. Seis novos verificam causalidade/atraso, campos por diferenças dos potenciais (incluindo a rampa), divergências/curls de Maxwell fora da fonte, transversalidade/fluxo distante, padrão sin²(theta), queda 1/r² da energia de radiação, máscara/finitude e parâmetros antes da alocação.

Viewer: 33 testes passaram, incluindo máscara nas linhas, campo vazio, espaçamento físico, escolha vetorial no cache e redução automática de isosuperfície. Não é validação de antena real nem estudo de convergência das linhas RK2. Float32 e LOD podem perder detalhes.

Quando a malha passa de 100000 triângulos, o worker reduz automaticamente a grade por interpolação e recalcula uma superfície completa. Não publica malha truncada. A interface informa dimensões reduzidas; campos originais em disco/cache ficam intactos. A redução pode apagar detalhes pequenos. Processamento é cancelável.
