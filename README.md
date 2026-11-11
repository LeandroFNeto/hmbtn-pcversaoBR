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
| — | `pc-port-item-art-debug` | A busca do `.ANI` da caixa de vendas (`0x064`) na primavera registra `exit absent`, com `list_count 35`, `count 35` e `start 0`: o id não está na lista. A casa (`0x11e`) cai na mesma saída. O desenho da caixa não muda |
| — | `pc-port-texture-mods` | Um BMP `pageX_pageY_u0_v0_u1_v1.bmp` em `mods/textures` ao lado do executável, ou na pasta de `HMBTN_MODS`, cobre a primitiva dessa página e UV. Sem o arquivo, a primitiva continua na VRAM |
| — | `pc-port-model-textures` | Um BMP `tmd_` mais oito dígitos hexadecimais, na mesma pasta, cobre só o TMD que montou o pacote, antes do arquivo de página e UV. A primeira vez o log escreve `modeltex: tmd_........`. A VRAM não muda |
| — | `pc-port-background-artifacts` | Um quad semitransparente branco na taxa 2 sobre branco lê `0,0,0`. Na lembrança (`f8-001.bmp`, `scene 0xe6`) o centro é `cmd 0x2a rgb 20,0,0 semi 1 abr 2`, pixel `13,25,16`. Na casa (`f8-002.bmp`, `scene 0x50`) o centro é `cmd 0x2c`, semi `0`. A máscara de cor estava aberta e nenhum clear veio depois. O pixel não foi trocado |
| — | `pc-port-album-capture` | Uma cópia de 8×8 do quadro desenhado para `(896, 256)` grava `0x001f`, e um sprite de 16 bits dessa célula desenha essa cor. `StoreImage` do scratch continua devolvendo a palavra que já estava lá |
| — | `pc-port-texture-dumper` | Com `HMBTN_DUMP_TEXTURES=1`, um F8 grava as primitivas texturizadas daquele quadro em `mods/textures/dump` como `pageX_pageY_u0_v0_u1_v1.bmp`. Segurar F8 não grava o seguinte, e um `0x0000` continua preto |
| — | `pc-port-final-geometry-fixes` (arquivado) | O nclip ficou o do PlayStation: inverter o sinal apagou o mapa 3D e deixou só o sprite, e o `gte.c` voltou ao produto vetorial original. No comando `0xE1` o lote é descarregado antes de trocar a taxa, e o quad sem textura semitransparente usa essa taxa. O `HMBTN_SELFTEST=1` passa em `scenery` e `bgtrace`. A cama da casa continua fora do nclip (`drawn 0`) |
| — | `pc-port-force-props` | A caixa e a porta da fazenda leem a cor do vértice no `HMBTN_SELFTEST=1` quando a amostra é `0x0000` e não há BMP. Um BMP dessa chave ainda ganha. Um `TMD_PRIM_FT4` construído no lugar da cama lê a cor guardada no pacote (`200,40,20`) e o nclip segue o do PlayStation (frente `10000`, costas `-10000`) |
| — | `pc-port-area-item-set` | A caixa só usa essa cor na fazenda, no galinheiro e no celeiro, e a porta só na fazenda. Um retângulo maior que apenas contém o ponto medido continua descartado. O `HMBTN_SELFTEST=1` lê `128,128,128` na caixa nessas três áreas, a cor limpa fora delas, e a cor limpa no retângulo maior |
| — | `pc-port-bin-picture` | Um polígono construído da caixa lê a cor da imagem do item na fazenda, no galinheiro e no celeiro. Um polígono vizinho continua na amostra calculada |
| — | `pc-port-model-art` | Um modelo de objeto construído e um modelo de personagem construído leem cada um a própria imagem do item. Um modelo sem regra continua na amostra calculada |
| — | `pc-port-map-geometry-export` (arquivado) | O self-check grava o JSON e o SVG de uma cena construída, e `(0, 0, 1000)` projeta em `(160, 120)`. Um script nulo volta erro; um modelo nulo, um primitivo sem formato ou um TMD que acaba no meio ainda grava o arquivo e o host segue |
| — | `pc-port-rtv0-translation` | `gte_rtv0` soma a translação (TR), como o `0x4A480012` do jogo (RTV0TR). Antes o host só rodava a rotação, e os objetos desenhados por `scene_spin_entries` (porta, baú e a tampa, cama, caixa de vendas) caíam atrás da câmera e o `rtpt` os descartava (`drawn 0`). Agora aparecem no lugar certo e animam. O quad forçado da cama (`force_bed`), o remendo de `model.c` no `host.cmake` e o `nclip` forçado foram removidos; o `nclip` segue o produto vetorial do PS1 |
| — | `pc-port-farm-projection` | Depois de salvar e dormir, a fazenda restaura o offset do GTE em `(160, 120)` (`farm_projection.c`, `--wrap=farm_enter_dispatch`). Antes o dia seguinte começava com a câmera fora de quadro, porque o host não recarrega o overlay e a abertura da fazenda não roda de novo |
| — | `pc-port-scenery-fixes`, `pc-port-invisible-objects`, `pc-port-dynamic-tmd-vram` (arquivados) | Verificado no jogo em 2026-11-10: o chão à esquerda da casa, o fundo da lembrança, a cama, o baú, a caixa de vendas e as portas da casa e do galinheiro aparecem com a própria textura |

### Ainda falta

- **Foto no álbum**: a cópia construída grava a cor, mas uma foto tirada no jogo ainda não foi conferida. `pc-port-album-capture` tarefa 2.1 aberta. Sem a linha `album:` nessa sessão, o jogo não copiou o draw buffer para `(896, 256)`.
- **Build no Linux**: `pc-port-psyq-stubs` tarefa 5.2 aberta (o WSL desta máquina não tem GCC; `cmake/host.cmake` ainda tem flags só de Windows).
- **Detalhes de GPU**: linhas, dithering e bit de máscara. A janela de textura já entra no quadro (`pc-port-gpu-fixes` tarefas 2.1 e 2.2).
- **Áudio**: reverb, pitch bend/vibrato, streaming CD-XA, arquivos SEP.
- **Luzes do Gs**: `GsSetFlatLight`, `GsSetAmbient` e `GsSetLightMode` continuam sem efeito no quadro. `pc-port-invisible-objects` tarefa 1.1 guarda os argumentos só para o despejo de um quadro. Um polígono `*_RAW` que já tem cor não é recolorido. O jogo não emite `ncs` nem `dpcs`.
- **Limites do cartão de memória**: o slot 2 nunca tem cartão, a PocketStation não existe, e editar o `.mcr` com o host aberto não é suportado (o host guarda a imagem na memória e grava por cima). Ficaram fora do escopo de `pc-port-memory-card`.
- **Mods** (PNG, retrato, roupas sazonais): o BMP de uma página e UV já cobre a primitiva (`pc-port-texture-mods`). PNG, o retrato e as roupas sazonais ficam de fora desse change.
- **Introdução com o prefeito**: na primeira cena de um jogo novo (`place 1`, `scene 0x0`, `f8-001.bmp`) a casa, o chão e as construções saem brancos, sem cor nem textura; só os sprites aparecem. O F8 gravou `tmdvram: sample cmd 0x2c page 960,256 uv 95,0 raw 0x0000` e `LoadImage` em `(992, 256)`, 16×46. Ainda não há change; é o próximo.
- **Foto no álbum**: adiada. `pc-port-album-capture` tarefa 2.1 continua aberta até uma foto tirada no jogo gerar a linha `album:`.
- **Arquivamento**: ainda abertos no OpenSpec, `pc-port-album-capture` (2.1) e `pc-port-psyq-stubs` (5.2, Linux). Os demais foram arquivados em 2026-11-10.

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

Diagnóstico: `HMBTN_SELFTEST=1` roda as checagens automáticas na abertura; `HMBTN_EXPORT_MAPS=1`, ou `--export-maps`, grava `debug/maps/scene_<id>_mesh.json` e `scene_<id>_wireframe.svg` uma vez para cada cena carregada. Um id como `0x50` grava só essa cena. A linha de comando ganha da variável. `HMBTN_DISABLE_EVENTS=1` faz o host, a cada leitura do pad e a cada `VSync`, ligar os bits de debug que pulam o dispatcher e os gatilhos de evento, e zera o cadeado do personagem (script, pad, troca de cena, pausa, transição e câmera) para o quadro seguinte. O fade da cena continua. `HMBTN_AUDIO_DUMP=arquivo.wav` grava os primeiros 20 s de áudio; `HMBTN_TRACE=1` registra as trocas de cena e as chamadas de cartão de memória; `HMBTN_OT_DUMP=1` arma o despejo dos objetos da cena: cada toque de F8 grava o quadro seguinte, e segurar não grava outro. Cada toque, com ou sem essa variável, grava só uma captura: `f8-001.bmp` e uma linha em `f8-captures.txt` ao lado do executável, por cima da anterior. `HMBTN_F8_LIST=1` volta à lista numerada (`f8-001.bmp`, `f8-002.bmp`, na ordem da sessão, e uma linha por toque). O log repete a linha `f8 capture N`. `HMBTN_OBJECTS=1` usa o mesmo F8: cada toque grava a fila de `DrawOTag` do quadro da fazenda que está na tela (comandos que cobrem a mancha preta, descarte do GTE, cabeçalhos do TMD e luzes). Segurar a tecla não grava o quadro seguinte. `HMBTN_TMD_VRAM=1` arma o despejo da textura dos modelos da cena: cada toque de F8 grava outro quadro, no log `build/host/hmbtn-host.log`, cada primitivo texturizado que cobre a mancha preta e amostra `0x0000`, com a página, o UV, a CLUT e o último retângulo de `LoadImage`, `MoveImage` ou `ClearImage`. Sem mancha preta, ou com uma mancha da altura inteira da tela, grava os que cobrem o centro da tela. Se nenhum cobre o centro, aquele toque não grava, e o seguinte tenta de novo. Se a mancha existe e nenhuma primitiva texturizada em cima dela amostra `0x0000`, o mesmo toque ainda grava as outras primitivas do quadro que amostram `0x0000` (`tmdvram: blob clear ignored`). `tmdvram: skip blob clear` fica só quando o quadro inteiro não tem essa primitiva. O mesmo toque grava `f8-NNN-map.bmp`, a foto com o contorno e o número de cada um desses polígonos, e `f8-NNN-map.txt` com a página, o UV, o `tmd_` e os cantos na tela. Uma busca de arte que volta vazia para a caixa de vendas (`0x064`), ou para qualquer item enquanto a cena é uma casa (`0x50` a `0x5B`), grava uma linha `itemart:` nesse mesmo log `build/host/hmbtn-host.log`, com a saída da busca e as palavras `count` e `start` da tabela. Entrar no mapa da fazenda carrega a arte da caixa sem outra chamada. Os outros mapas não carregam. `HMBTN_MODS` aponta a pasta dos BMP de textura; sem essa variável a pasta é `mods/textures` ao lado do executável. Um arquivo `pageX_pageY_u0_v0_u1_v1.bmp` casa com essa página e esses UV. Um arquivo `tmd_` mais oito dígitos hexadecimais, na mesma pasta, é tentado antes desse arquivo de página e UV. A linha `modeltex:` no log é o nome do arquivo. A taxa 2 de um quad branco sobre branco leu `0,0,0`, e a equação ficou. `HMBTN_BG_TRACE=1` e um F8 na casa (`0x50` a `0x5B`) ou na lembrança (`place 54`, `scene 0xe6` ou `0xe7`) escrevem `bgtrace:` com o comando da borda, a cor crua, o bit semi, a taxa e se um clear veio depois. O pixel não é trocado. `HMBTN_DUMP_TEXTURES=1` arma o extrator no mesmo F8: um toque grava as primitivas texturizadas daquele quadro em `mods/textures/dump` ao lado do executável, cada arquivo `pageX_pageY_u0_v0_u1_v1.bmp` com a cor já armazenada, e um `0x0000` continua preto. Segurar F8 não grava o quadro seguinte. Essa pasta não é lida pelo carregador. O log fica em `build/host/hmbtn-host.log`. Num crash, a linha `host fault stack:` traz os endereços de retorno, que `addr2line -f -p -e build\host\hmbtn-host.exe <endereços>` transforma em funções e linhas.