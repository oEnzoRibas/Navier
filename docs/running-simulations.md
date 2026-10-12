# Rodando as duas simulações

O Navier armazena e indexa estados. Os executáveis de calor e fluido são produtores externos: calculam seus campos e chamam `CheckpointStore::save()`. O solver não acessa a B+ Tree.

## Compilação no Windows

Na pasta do projeto, com Visual Studio 2026 instalado:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Release --parallel 1
ctest --test-dir build-msvc -C Release --output-on-failure
```

GoogleTest precisa estar instalado ou ser baixado pelo CMake na primeira configuração. Para usar uma cópia local, acrescente `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=C:/caminho/googletest` à configuração. Em outros ambientes, use o gerador e compilador disponíveis; o CMake também oferece configuração Ninja.

## Execução

```powershell
.\build-msvc\Release\navier_heat2d.exe heat2d.data 64 400 20
.\build-msvc\Release\navier_fluid2d.exe fluid2d.data 64 400 20
```

Argumentos: arquivo de saída, lado da malha, número de passos, intervalo entre checkpoints. A execução acima usa 64×64 células e salva os passos 0, 20, ..., 400: 21 checkpoints por arquivo. O passo final é salvo mesmo que não seja múltiplo do intervalo. A pasta de saída deve existir. Escolha novos nomes ao repetir uma execução: os produtores rejeitam arquivos existentes.

Cada arquivo recebe um sidecar `.data.json` com dimensões, campos, passo de tempo e condições de contorno. Esse metadado é responsabilidade do exemplo, mantendo o formato do núcleo independente da física.

## Difusão de calor 2D

Equação: `dT/dt = alpha * (d²T/dx² + d²T/dy²)`, com `alpha = 0.1`.

Domínio `[0,1]×[0,1]`, bordas em temperatura zero e estado inicial `T(x,y)=sin(pi*x)*sin(pi*y)`. O stencil usa diferenças finitas de cinco pontos e Euler explícito. `dx=1/(N-1)` e `dt=0.2*dx²/alpha`, respeitando o limite de estabilidade `alpha*dt/dx² <= 1/4`. Com 64 pontos e 400 passos, o tempo final é aproximadamente 0.201562, em unidades consistentes com alpha e o domínio.

O modo inicial permite comparar com a solução contínua `T(x,y,t)=T(x,y,0)*exp(-2*pi²*alpha*t)`. Os testes verificam também a solução discreta e o princípio do máximo. Não há fonte de calor nem condições de contorno configuráveis nessa versão.

Payload: `N*N` valores de temperatura, índice `y*N+x`.

## Escoamento de fluido 2D

Método LBM D2Q9 com colisão BGK e streaming periódico. O caso é um vórtice de Taylor–Green, com velocidade inicial máxima de 0.03 e viscosidade cinemática 0.02, em unidades de lattice (`dx=dt=1`). A relaxação usa `tau=0.5+3*nu`. A densidade inicial inclui a pequena variação de pressão do vórtice.

O escoamento é fracamente compressível e de baixo Mach. O caso periódico serve para verificar conservação de massa e momento e dissipação da energia cinética. Não representa escoamento em canal, obstáculos, cavidade ou turbulência; esses casos exigiriam novas condições de contorno e validação.

Payload: três planos de `N*N` valores, nesta ordem: densidade, velocidade x, velocidade y. Cada plano usa índice `y*N+x`. O tempo final de 400 passos é 400 unidades de lattice; não é o mesmo tempo físico do exemplo de calor.

## Consultar checkpoints com C++

```cpp
navier::storage::CheckpointStore store("fluid2d.data");
auto final_state = store.load(400);
auto checkpoints = store.range(200, 400); // limites inclusivos
```

O índice é reconstruído em RAM na abertura. `load()` retorna metadados e o vetor numérico. A interpretação de planos/células vem do sidecar e do produtor. Uma chave ausente lança `std::out_of_range`.

## Visualizar agora

A GUI interativa é uma evolução futura. Já é possível exportar uma figura estática dos checkpoints:

```powershell
python -m pip install numpy matplotlib
python examples/plot_2d.py heat2d.data fluid2d.data --output simulations.png
```

A figura compara os estados inicial e final com a mesma escala de cores por simulação; o fluido inclui setas da velocidade. O script também imprime erro do calor frente à solução contínua, massa e energia do fluido. Ele lê o formato binário diretamente e não constitui bindings Python da biblioteca. Os pacotes Python são opcionais.

## Explorar os arquivos com o executável Navier

Depois de salvar os checkpoints, use diretamente a ferramenta do Navier:

```powershell
.\build-msvc\Release\navier.exe info heat2d.data
.\build-msvc\Release\navier.exe get fluid2d.data 400
.\build-msvc\Release\navier.exe range heat2d.data 200 300
```

`info` mostra quantidade de checkpoints, tamanho do arquivo e registros iniciais. `get` recupera o checkpoint escolhido e mostra metadados, quantidade de valores e uma prévia do payload. `range` lista os estados salvos no intervalo inclusivo. Cada comando abre o arquivo existente e reconstrói o índice B+ Tree em RAM; o solver não precisa estar rodando.

Essa é a interface de terminal do Navier. A visualização estática continua no script `plot_2d.py`; a interface gráfica interativa segue como etapa futura. Os comandos não alteram registros, mas a API atual abre o stream com acesso de leitura e escrita. Arquivos inexistentes são rejeitados sem criação. Código de saída 2 indica checkpoint ausente; 1 indica outros erros.
