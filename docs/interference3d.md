# Interferência entre duas antenas 3D

Dois dipolos elétricos ideais paralelos ao eixo Z, posicionados em X=−d/2 e X=+d/2. Cada um tem sua distância e tempo retardado. O segundo oscila com uma diferença de fase constante, escolhida ao gerar o arquivo. Modelo analítico de espaço livre, não de fios metálicos ou acoplamento de antenas reais. Base física: [Feynman I, seção 29-4](https://www.feynmanlectures.caltech.edu/I_29.html#Ch29-S4) e [o modelo de dipolo](antenna3d.md).

## Rodar

```powershell
cmake --build build-msvc --config Release --target navier_interference3d
.\build-msvc\Release\navier_interference3d.exe interferencia_nova.data 48 600 6 90 4
.\start_viewer.cmd
```

Argumentos: `[arquivo] [lado] [passos] [intervalo] [fase-em-graus] [separação]`. Padrão: 48³, 600 passos, salvamento cada 6, fase 90°, separação 4. Período 4, dt=.05, c=ε=μ=1; comprimento de onda 4. Dois turn-ons suaves C2 de duração 4. Fase constante durante cada execução, não um slider que modifica dados já salvos. Use novo nome para cada execução.

Para comparar fases:

```powershell
.\build-msvc\Release\navier_interference3d.exe fase0.data 48 600 6 0 4
.\build-msvc\Release\navier_interference3d.exe fase180.data 48 600 6 180 4
```

As restrições de malha e tempo são as da antena anterior. Com a janela padrão use lado 25–96. Fase CLI em [−360,360]. Ambas as esferas de exclusão devem caber na janela; separação zero é permitida como caso de validação.

## Visualização

Exemplo gerado em `runs/showcase/interference3d.data`: **715030334 bytes**, 101 checkpoints, 16 campos float32 e métricas. `validate full` passou; arquivo/índice permanecem fora do Git.

Abra em Exemplos locais. Configuração conferida no browser:

- `electric_z`, modo Volume, mapa Divergente.
- Escala manual −.03 a .03, simetria ativada.
- Limiar baixo .003 e Aplicar limiar a |campo| ativado.
- Step 480 (t=24); clique Reproduzir e orbite livremente.

`radiation_interference` mostra diretamente o termo de interferência da contribuição 1/r: positivo significa reforço energético relativo às fontes isoladas; negativo significa redução. Use mapa Divergente, corte Z central ou uma isosuperfície positiva/negativa. `interference_energy` inclui também campo próximo. `energy_density` permanece não negativa. Electric_z representa sinal de um componente de campo, não classifica sozinho interferência construtiva/destrutiva.

Linhas de campo E/B/Poynting e potenciais continuam disponíveis, com seleção vetorial explícita. Isosuperfície conserva fallback automático de resolução. O interior das duas fontes é NaN com source_mask=0; não é um defeito de geração.

## Superposição e campos

E=E₁+E₂, B=B₁+B₂, phi=phi₁+phi₂ e A=A₁+A₂. Depois da soma:

```text
u = (|E|² + |B|²)/2
u_independente = u1 + u2
u_interferencia = E1·E2 + B1·B2
u = u_independente + u_interferencia
S = E × B
```

Somar energias isoladas não produz interferência. A energia radiativa usa os campos 1/r somados, incluindo E_rad e B_rad; para fontes em direções distintas não se presume |E_rad_total|=|B_rad_total|.

Campos: electric_x/y/z, magnetic_x/y/z, poynting_x/y/z, potential, energy_density, radiation_energy, source_mask, interference_energy, incoherent_energy, radiation_interference. Preserva os campos antigos e acrescenta três diagnósticos. A avaliação é double; snapshots são float32.

Métricas: dipole_a, dipole_b, phase_degrees, energy_in_window, interference_in_window. Energias integradas são quadraturas na janela fora das máscaras; não provam conservação global. Checkpoints analysis, sem retomada de solver. Ondas saem da janela; as fontes são prescritas e não reagem ao campo uma da outra.

## Verificação

100 testes passaram em Release. Em Debug passaram os 11 testes de antena/interferência. Os cinco novos conferem energia quadruplicada para fontes coincidentes em fase, cancelamento de fase oposta, identidade com termos cruzados e sinais positivos/negativos, Poynting total, atrasos/máscaras independentes, resíduos de Maxwell fora das fontes e rejeição de parâmetros.

Viewer existente sem alterações nesta etapa. Browser conferiu abertura real, cores e reprodução. Amostragem/LOD e linhas RK2 continuam aproximações visuais; não é estudo de convergência de uma antena física.
