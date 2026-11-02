# Harvest Moon: Back to Nature - PC Port & Modding Framework

> **Nota do Desenvolvedor (Fan Fork):** Este repositório é uma ramificação (fork) do projeto original de descompilação do *Harvest Moon: Back to Nature*. Como fã da franquia, o objetivo principal desta versão é construir uma **Camada de Abstração de Hardware (HAL)** para rodar o jogo nativamente no PC (Windows/Linux), sem o uso de emuladores.
>
> Ao libertar o código das limitações físicas do hardware do PlayStation 1 (como o gargalo extremo de VRAM e as bibliotecas proprietárias da Sony), a intenção é criar uma fundação sólida onde a criação de *mods* seja infinitamente mais acessível. O objetivo de longo prazo é permitir a injeção de roupas sazonais, melhorias no design da casa, texturas em alta resolução e o rompimento dos limites da engine clássica.

---

## O Projeto Base (Descompilação PS1)

Este fork mantém a base da descompilação exata de *Harvest Moon: Back to Nature* (PlayStation, USA, `SLUS_011.15`). O código fonte em C é capaz de compilar o executável (EXE) e seus 13 overlays byte a byte e, desde que você forneça a sua própria cópia do disco, recriar a imagem ISO original.

## Compilando a ROM Original (PS1)

```sh
docker build -t hmbtn-build .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src:ro -v /tmp/hmbtn-build:/build \
    -v /path/to/disc:/disc:ro hmbtn-build bash -c 'cmake -S /src -B /build -G Ninja \
    -DORIGINAL_DISC=/disc/disc.bin && cmake --build /build --target check'
```

## Status do Port para PC

O port roda o código original do jogo num executável nativo (`hmbtn-host.exe`, MinGW 32 bits + SDL2 + OpenGL 2.1). Nenhum arquivo em `src/main/` ou `src/overlay/` é editado; tudo o que é do PC fica em `src/pc_port/` e `cmake/host.cmake`. O planejamento de cada fase está em `openspec/changes/`.

### Já resolvido

| Fase | Change (OpenSpec) | O que funciona |
| --- | --- | --- |
| 1 | `pc-port-psyq-stubs` | Todo o código do jogo compila e linka no PC contra stubs das bibliotecas Psy-Q |
| 2 | — | Janela SDL2 640×480 que fecha pelo botão |
| 3 | `pc-port-gl-renderer` | Contexto OpenGL, `DrawOTag` desenhando primitivas, GTE em C |
| 4 | `pc-port-ps1-memory`, `pc-port-vsync-loop` | RAM do PS1 mapeada em `0x80000000`, elos de 24 bits, `ps1_main()` rodando na thread principal com o `VSync` como fronteira de quadro |
| 5 | `pc-port-cd-drive` | Leitura dos arquivos reais do disco (`baserom.bin` extraído no build) |
| 6 | `pc-port-vram` | VRAM 1024×512, texturas e paletas, semitransparência: a tela de título aparece completa |
| 7 | `pc-port-input` | Teclado e controle (SDL2): título → menu → New Game → diálogo do prefeito → tela de nome |
| 8 | `pc-port-audio` | SPU virtual, ADPCM (VAG), ADSR e efeitos sonoros pelo SDL2 |
| 9 | `pc-port-seq-player` | Música SEQ com tempo por amostras e loop (trilha do título) |
| 10 | `pc-port-memory-card` (arquivado) | Cartão de memória virtual no slot 1 (`mcd001.mcr`, 128 KB no formato bruto do PS1, o mesmo dos emuladores): gravar no diário cria o save de 4 blocos, fechar e abrir o host mantém o arquivo, e o CONTINUE lista o diário com nome e data e volta para dentro da casa no dia salvo |
| 11 | `pc-port-farm-load` (arquivado) | As tabelas de animação da fazenda guardam seus offsets no PC (cópia corrigida de `anim.h`, conferida pelo `HMBTN_SELFTEST=1`), o build falha se outro arquivo perder inicializadores do mesmo jeito, e o jogo passa do travamento em `anim_bind_file` e entra na cena da fazenda |
| 12 | `pc-port-null-reads` (arquivado) | Leituras e escritas por ponteiro nulo que o PS1 tolera (endereço 0 é RAM do kernel) chegam à RAM baixa do PS1 no PC, com os valores do kernel; as do primeiro quadro da fazenda em `scene.c` passam. Um crash agora registra a pilha no log |
| 13 | `pc-port-gte-library` (arquivado) | As 22 funções da `libgte` que o jogo chama (seno/cosseno, arco-tangente, raízes, matrizes de rotação, pilha de matrizes, `RotTrans`, normalização, neblina) dão os mesmos resultados da biblioteca do PS1, conferidos no `HMBTN_SELFTEST=1` contra as tabelas do próprio executável: um jogo novo chega à fazenda, o prefeito vem falar com o jogador e o jogo segue rodando |
| 14 | `pc-port-ground-probe` (arquivado) | Quando a sonda de chão não acha chão (um passo à frente do jogador, ou numa varredura de campo), o PC lê o mesmo "sem chão" que o PS1: a lembrança de infância da primeira manhã passa e o jogo chega ao jogo livre dentro da casa |
| — | `pc-port-gpu-fixes` (arquivado) | `StoreImage` sobre o draw buffer devolve o quadro em 15 bits (o álbum em `(896, 256)` continua na VRAM estática), a janela de textura `0xE2` mascara o UV, e `SetBackColor` com as duas matrizes guarda o valor. O `HMBTN_SELFTEST=1` cobre os três. As áreas pretas continuam: o traço da tarefa 4.2 não casou com nenhuma das três correções |
| — | — | O ajudante da colheita avança o quadro: o tick passa o helper para o cue (no PS1 o ponteiro ainda estava em `a0`). Sem isso o host lia `taskId` no endereço `0x2A` ao carregar a fazenda. O log depois do patch mostra quadros da casa (`scene 0x50`) |

### Ainda falta

- **Build no Linux**: `pc-port-psyq-stubs` tarefa 5.2 aberta (o WSL desta máquina não tem GCC; `cmake/host.cmake` ainda tem flags só de Windows).
- **Detalhes de GPU**: linhas, dithering e bit de máscara. A janela de textura já entra no quadro (`pc-port-gpu-fixes` tarefas 2.1 e 2.2).
- **Áudio**: reverb, pitch bend/vibrato, streaming CD-XA, arquivos SEP.
- **Caixa de vendas, baú e portas invisíveis**: objetos TMD da cena, separados do chão e dos fundos. O despejo de um quadro está no host (`HMBTN_OT_DUMP=1`, F8). Falta a captura parado em frente à caixa de vendas: `pc-port-missing-objects` tarefa 5.1. Nenhuma correção foi aplicada.
- **Áreas pretas no cenário**: à esquerda da casa, no lugar da cama e no fundo da lembrança. O traço de `pc-port-gpu-fixes` tarefa 4.2 não casou com janela de textura, amostra no endereço errado, nem linha/preenchimento engolindo o pacote (`e2`, `line` e `fill` ficaram 0). A cama não tem primitiva no centro; o buraco da fazenda é uma quad sem textura `(20, 0, 0)` ou um sprite 8 bits cuja amostra é `0x0000`. Nenhuma das três correções foi aplicada.
- **Objetos invisíveis**: o baú, a caixa de vendas e as portas da casa e da fazenda não aparecem, e a lembrança ainda mostra essas formas pretas. O despejo da casa (`scene 0x50`) parou no ramo E de `pc-port-invisible-objects`: o polígono é desenhado com `rgb 128,128,128` e a amostra é `word 0x0000`. A correção da textura está em `pc-port-dynamic-tmd-vram` (tarefas 1.1, 1.2, 1.3, 2.1 e 3.1 feitas; 4.1 e 4.2 abertas). O F8 no quadrado preto gravou `LoadImage` em `(368, 242)` e `(368, 246)`, 14×1, e a palavra lá é `0x0000`. Nenhuma cor foi inventada. As tarefas 2.1, 2.2, 3.1, 4.1 e 4.2 de `pc-port-invisible-objects` continuam abertas e não ganham outra teoria.
- **Luzes do Gs**: `GsSetFlatLight`, `GsSetAmbient` e `GsSetLightMode` continuam sem efeito no quadro. `pc-port-invisible-objects` tarefa 1.1 guarda os argumentos só para o despejo de um quadro. Um polígono `*_RAW` que já tem cor não é recolorido. O jogo não emite `ncs` nem `dpcs`.
- **Limites do cartão de memória**: o slot 2 nunca tem cartão, a PocketStation não existe, e editar o `.mcr` com o host aberto não é suportado (o host guarda a imagem na memória e grava por cima). Ficaram fora do escopo de `pc-port-memory-card`.
- **Mods** (`/mods`, texturas HD, roupas sazonais): os pontos de encaixe existem (`vram_observe_write` e `texture_resolve` em `src/pc_port/src/vram.c`), mas nenhum carregador.
- **Arquivamento**: os changes acima, exceto `pc-port-gl-renderer`, `pc-port-farm-load`, `pc-port-null-reads`, `pc-port-gte-library`, `pc-port-memory-card`, `pc-port-ground-probe` e `pc-port-gpu-fixes`, ainda não foram arquivados no OpenSpec.

## Compilando o Port para PC (Windows)

Requisitos: CMake ≥ 3.20, MinGW GCC 32 bits, o pacote `SDL2-devel-2.x-mingw` (pasta `i686-w64-mingw32`), Python ≥ 3.10 e a sua imagem do disco em `baserom.bin` na raiz do repositório (ela está no `.gitignore`).

```bat
cmake -S . -B build/host -DHMBTN_HOST=ON -DHMBTN_RUN_GAME=ON -G "MinGW Makefiles" ^
    -DCMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe ^
    -DSDL2_DIR=C:/SDL2/SDL2-2.32.10/i686-w64-mingw32/lib/cmake/SDL2
cmake --build build/host
build\host\hmbtn-host.exe
```

O build falha se um arquivo do jogo gerar o aviso "excess elements in array initializer": o GCC 2.95 do PS1 guardava esses valores e o GCC moderno os descarta. A correção é uma cópia corrigida do header, com `hmbtn_host_patch_header` em `cmake/host.cmake`, sem editar `src/`.

Teclado: setas = direcional, `X` = ✕, `Z` = ○, `C` = □, `V` = △, `Enter` = Start, `Shift direito` = Select, `A`/`S` = L1/R1, `Q`/`W` = L2/R2.

Cartão de memória: o slot 1 é o arquivo `mcd001.mcr` ao lado do executável, criado já formatado na primeira execução; `HMBTN_MEMCARD=caminho\arquivo.mcr` usa outro arquivo. É uma imagem bruta de 128 KB, que pode ser copiada de ou para emuladores (DuckStation, ePSXe).

Diagnóstico: `HMBTN_SELFTEST=1` roda as checagens automáticas na abertura; `HMBTN_AUDIO_DUMP=arquivo.wav` grava os primeiros 20 s de áudio; `HMBTN_TRACE=1` registra as trocas de cena e as chamadas de cartão de memória; `HMBTN_OT_DUMP=1` arma o despejo de um quadro dos objetos da cena, e F8 grava o quadro seguinte. `HMBTN_OBJECTS=1` usa o mesmo F8: cada toque grava a fila de `DrawOTag` do quadro da fazenda que está na tela (comandos que cobrem a mancha preta, descarte do GTE, cabeçalhos do TMD e luzes). Segurar a tecla não grava o quadro seguinte. `HMBTN_TMD_VRAM=1` arma o despejo da textura dos modelos da cena: F8 grava, no log `build/host/hmbtn-host.log`, cada primitivo texturizado que cobre a mancha preta e amostra `0x0000`, com a página, o UV, a CLUT e o último retângulo de `LoadImage`, `MoveImage` ou `ClearImage`. Sem mancha preta, ou com uma mancha da altura inteira da tela, grava os que cobrem o centro da tela. Se nenhum cobre o centro, o despejo continua armado. O log fica em `build/host/hmbtn-host.log`. Num crash, a linha `host fault stack:` traz os endereços de retorno, que `addr2line -f -p -e build\host\hmbtn-host.exe <endereços>` transforma em funções e linhas.