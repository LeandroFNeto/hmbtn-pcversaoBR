# final-geometry Specification

## Purpose

Define a decodificação de semitransparência para primitivas não-texturizadas e a preservação do normal clipping canônico do GTE.

## Requirements

### Requirement: Untextured quad blending
Primitivas `POLY_G4` e `POLY_F4` (`cmd 0x2a` / `0x28`) SHALL aplicar blending subtrativo ou aditivo do OpenGL quando emitidas com o bit de semitransparência ativo, fundindo-se com o fundo renderizado.

#### Scenario: Untextured quad blends over background
- **WHEN** uma primitiva `cmd 0x2a` é emitida sobre um pixel do framebuffer
- **THEN** o OpenGL aplica a equação de blend configurada pelo modo semitransparente em vez de preenchimento opaco

### Requirement: Scenery background clarity
O terreno da fazenda e a cena de flashback SHALL NOT exibir quads ou blocos pretos opacos sobre o cenário.

#### Scenario: Farmhouse and flashback scenery render without black boxes
- **WHEN** o jogador visualiza o terreno à esquerda da fazenda ou a cena de flashback
- **THEN** o fundo renderiza sem oclusão por polígonos pretos sólidos

### Requirement: Canonical normal clipping
O discriminante de `gte_h_nclip` SHALL permanecer o produto vetorial canônico do PlayStation. Uma inversão global do sinal SHALL NOT ser usada para forçar a malha da cama.

#### Scenario: Bed mesh is outside the clip test
- **WHEN** a câmera visualiza o quarto da casa (`scene 0x50`) e o grupo mais perto da cama registra `nclip_fail 0`, `nclip_pass 0` e `drawn 0`
- **THEN** o host mantém o nclip canônico e a cama continua fora desse teste de face

### Requirement: Telemetry transparency validation
O rastreador isolado (F8 Dump) SHALL registrar os pacotes `cmd 0x2a` com seus parâmetros de mistura ativos.

#### Scenario: F8 dump logs semi mode for untextured primitives
- **WHEN** a tecla F8 é acionada sob `HMBTN_OBJECTS=1` em cena com quad semitransparente
- **THEN** o log registra o comando com o modo de semitransparência decodificado
