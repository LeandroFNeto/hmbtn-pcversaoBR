# Design

## Context

O diagnóstico do subsistema gráfico confirmou que o motor de blending do hospedeiro ignora parâmetros de semitransparência em polígonos não-texturizados (`POLY_G4` / `POLY_F4`), acumulando lotes de vértices sem descarga prévia na alteração de `DR_MODE` (`0xE1`).

## Goals / Non-Goals

**Goals:**
- Manter o cálculo de `nclip` canônico e fiel ao hardware PS1 no `gte.c` (uma inversão global descartou o mapa 3D inteiro, provando que o winding order do mundo é o padrão MIPS).
- Forçar `flush_verts()` no decodificador de `0xE1` (`SetDrawMode` / `DR_MODE`) em `libgpu.c` para que primitivas não-texturizadas semitransparentes recebam a equação correta de mistura antes que novo modo seja ativado.
- Manter 100% de isolamento em relação ao código original em `src/`.

**Non-Goals:**
- Alterar grafos de cena ou lógica de gameplay em `src/main/` ou `src/overlay/`.
- Inverter globalmente o nclip do GTE.
- Alocar memória dinâmica no loop de renderização.

## Decisions

### 1. Preservação do GTE Canônico
A inversão global do sinal no discriminante de `nclip` resultou na eliminação visual de todo o cenário 3D (terreno, cercas, casas e árvores), mantendo unicamente o sprite 2D do protagonista em cena. Conclui-se que o winding order da geometria do jogo adere estritamente à convenção padrão de hardware da Sony. O cálculo canônico de `gte_h_nclip` em `gte.c` foi mantido integralmente.

### 2. Sincronização de Semitransparência no Decodificador OpenGL
No laço de processamento de comandos em `libgpu.c`, o recebimento de `0xE1` (`SetDrawMode` / `setDrawTPage`) agora força `flush_verts()` antes de registrar `st_abr` e a nova página. Isso impede que quads não-texturizados (`cmd 0x2a`) sejam emitidos com o estado herdado incorretamente do pacote seguinte ou anterior.

## Risks / Trade-offs

- [A cama continuar sem renderizar na cena 0x50] → Como `nclip_fail` e `nclip_pass` da cama são 0, a geometria sequer atinge a chamada de GTE nesta fase do grafo de cena do jogo decompilado. Sua exibição definitiva depende de renderização por modelo dedicado ou hook de overlay, sem corromper o culling global.
