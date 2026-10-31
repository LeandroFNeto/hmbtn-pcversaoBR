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

### Ainda falta

- **Build no Linux**: `pc-port-psyq-stubs` tarefa 5.2 aberta (o WSL desta máquina não tem GCC; `cmake/host.cmake` ainda tem flags só de Windows).
- **Renderizar para a VRAM**: efeitos que leem a tela de volta com `StoreImage` (captura do álbum de fotos) veem dados antigos.
- **Detalhes de GPU**: linhas, dithering, bit de máscara e janela de textura.
- **Áudio**: reverb, pitch bend/vibrato, streaming CD-XA, arquivos SEP.
- **Cartão de memória**: `libmcrd`/`libmcx` respondem "sem cartão"; não há save/load.
- **Gameplay além da introdução**: não verificado até a fazenda.
- **Mods** (`/mods`, texturas HD, roupas sazonais): os pontos de encaixe existem (`vram_observe_write` e `texture_resolve` em `src/pc_port/src/vram.c`), mas nenhum carregador.
- **Arquivamento**: os changes acima, exceto `pc-port-gl-renderer`, ainda não foram arquivados no OpenSpec.

## Compilando o Port para PC (Windows)

Requisitos: CMake ≥ 3.20, MinGW GCC 32 bits, o pacote `SDL2-devel-2.x-mingw` (pasta `i686-w64-mingw32`), Python ≥ 3.10 e a sua imagem do disco em `baserom.bin` na raiz do repositório (ela está no `.gitignore`).

```bat
cmake -S . -B build/host -DHMBTN_HOST=ON -DHMBTN_RUN_GAME=ON -G "MinGW Makefiles" ^
    -DCMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe ^
    -DSDL2_DIR=C:/SDL2/SDL2-2.32.10/i686-w64-mingw32/lib/cmake/SDL2
cmake --build build/host
build\host\hmbtn-host.exe
```

Teclado: setas = direcional, `X` = ✕, `Z` = ○, `C` = □, `V` = △, `Enter` = Start, `Shift direito` = Select, `A`/`S` = L1/R1, `Q`/`W` = L2/R2.

Diagnóstico: `HMBTN_SELFTEST=1` roda as checagens automáticas na abertura; `HMBTN_AUDIO_DUMP=arquivo.wav` grava os primeiros 20 s de áudio; o log fica em `build/host/hmbtn-host.log`.