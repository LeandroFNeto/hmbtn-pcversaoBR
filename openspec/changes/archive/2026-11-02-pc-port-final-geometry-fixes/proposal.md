# Proposal

## Why

O decodificador de GPU aplicava a taxa de semitransparência nova a primitivas que ainda estavam no lote, e uma inversão global do nclip descartou o mapa 3D inteiro. A cama da casa não entra nesse teste de face.

## What Changes

- `flush_verts()` ao receber `0xE1` em `libgpu.c`, antes de trocar a taxa, para que os quads sem textura (`cmd 0x2a` / `0x28`) usem a mistura do pacote em que foram emitidos.
- O `gte_h_nclip` permanece o produto vetorial canônico do PlayStation. A inversão de sinal foi testada e revertida.
- As alterações ficam em `src/pc_port/`.

## Capabilities

### New Capabilities
- `final-geometry`: mistura de polígonos sem textura e preservação do nclip canônico. A cama da cena `0x50` continua fora desse teste.

### Modified Capabilities

## Impact

- Modificações restritas a `src/pc_port/src/gte.c` e `src/pc_port/src/libgpu.c`.
- Zero alterações em `src/main/`, `src/overlay/` ou `src/include/`.
