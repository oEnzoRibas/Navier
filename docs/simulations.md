# Produtores e limites científicos

Navier não resolve as equações: produtores externos calculam e salvam estados. Use nomes novos a cada execução. Estes exemplos são educacionais; não são um pacote CFD ou uma certificação de precisão. Checkpoints dos novos produtores são analysis, não restart.

|Executável|Equação / método|Campos e validação / limites|
|---|---|---|
|heat2d|T_t=alpha Lap T; Euler explícito, cinco pontos, Dirichlet|temperatura; modo senoidal/discreto, máximo e estabilidade testados; 2D homogêneo|
|fluid2d|Navier–Stokes aproximado por LBM D2Q9 BGK periódico|rho,u,v; massa/momento e decaimento Taylor–Green; baixo Mach|
|cavity2d|Navier–Stokes incompressível, projeção e pressão Poisson, tampa móvel|velocidade/pressão; projeção e alta resolução testadas; parâmetros fixos, não turbulência validada|
|shear2d|LBM D2Q9, camada de cisalhamento periódica|rho/velocidade/vorticidade; estabilidade e invariantes existentes; limitações LBM|
|fluid3d|LBM D3Q19, vórtice ABC periódico|rho/velocidade; massa/momento e dissipação testados; lattice e baixo Mach|
|electromag2d/3d|Maxwell normalizado, atualização FDTD-style|energia/E e fluxo; exemplos herdados, sem nova validação analítica nesta etapa; nomes velocity_* do 3D representam Poynting, não velocidade de fluido|
|lorenz3d|sistema Lorenz, RK4 do exemplo herdado|densidade de ocupação do atrator; caos demonstrativo, sem novo estudo de convergência|
|wave3d / wave3d_v2|u_tt=c² Lap u+S; diferenças centrais de segunda ordem|amplitude/energy_density; repouso, CFL, simetria e bordas testados; energia diagnóstica não é invariante discreto exato|
|grayscott3d|u_t=Du Lap u-uv²+F(1-u); v_t=Dv Lap v+uv²-(F+k)v; Euler e Laplaciano 7 pontos periódico|species_u/species_v, mean_v/max_v; equilíbrio exato, determinismo, finitude, malha retangular e limites testados; falta convergência/referência independente|

Wave: dx=1, c dt<=1/sqrt(3), seis faces Dirichlet. Fonte pontual central senoidal com envelope temporal gaussiano; após desligar, ondas refletem. Não é condição inicial gaussiana espacial livre. V2 registra método/parâmetros/bordas, métricas max_amplitude/total_energy.

Gray–Scott: dimensões 8–128 em cada eixo, Du=.16,Dv=.08,F=.04,k=.06,dt=.25,dx=1. Esfera inicial central u≈.5/v≈.25 com pequena perturbação determinística; fora u=1/v=0. Condição de difusão 6 max(D)dt<=1 e limite conservador dt<=.25. Concentração negativa/não finita gera erro, sem clamp visual. Sistemas não lineares também precisam validação de reação; a condição difusiva isolada não prova precisão.

Exemplos pequenos de cada produtor (raiz do projeto):

```powershell
.\build-msvc\Release\navier_heat2d.exe heat_nova.data 64 400 20
.\build-msvc\Release\navier_fluid2d.exe fluid_novo.data 64 400 20
.\build-msvc\Release\navier_cavity2d.exe cavity_nova.data 64 1000 20
.\build-msvc\Release\navier_shear2d.exe shear_nova.data 64 1000 20
.\build-msvc\Release\navier_fluid3d.exe fluid3d_novo.data 32 400 20
.\build-msvc\Release\navier_electromag2d.exe em2d_novo.data 64 200 10
.\build-msvc\Release\navier_electromag3d.exe em3d_novo.data 32 120 10
.\build-msvc\Release\navier_lorenz3d.exe lorenz_novo.data 32 400 20
.\build-msvc\Release\navier_wave3d.exe wave_legacy_nova.data 32 160 10
.\build-msvc\Release\navier_wave3d_v2.exe wave_nova.data 32 160 10
.\build-msvc\Release\navier_grayscott3d.exe gray_novo.data 32 32 32 4000 100
.\start_viewer.cmd
```

Gray usa NX NY NZ separados; demais usam lado/steps/intervalo. Os demais ainda escrevem NAVIER01+sidecar. V2 Wave/Gray escrevem campos com metadados dentro do .data e índice .nvi. Quantum3D, Elastic3D e Thermal3D foram adiados para priorizar armazenamento e validação.

## Antena oscilante

Novo `navier_antenna3d`: dipolo ideal, solução analítica retardada de Maxwell, campos E/B/Poynting/potencial/energia em NAVIER02. Veja [equações, máscara, testes e execução](antenna3d.md).
