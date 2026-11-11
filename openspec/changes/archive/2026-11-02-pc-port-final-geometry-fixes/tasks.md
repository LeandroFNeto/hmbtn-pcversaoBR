# Tasks

## 1. [GTE] Auditoria do Teste de Face

- [x] 1.1 Testar inversão de Produto Vetorial de normais 2D no `gte.c` e verificar impacto global.
  - Resultado: a inversão de sinal descartou 100% dos polígonos front-facing do mapa 3D (terreno, paredes, construções), deixando visível apenas o sprite 2D do protagonista. O `gte.c` foi restaurado ao algoritmo canônico do hardware PlayStation.
- [x] 1.2 Confirmar que a ausência da cama na cena 0x50 não decorre de falha no nclip (telemetria registrou `nclip_fail 0 nclip_pass 0 drawn 0`, provando que o objeto sequer entra no pipeline de transformação vetorial nesta cena).

## 2. [OpenGL] Decodificação de Semitransparência

- [x] 2.1 Alterar `libgpu.c` para acionar `flush_verts()` no recebimento de `0xE1` (`SetDrawMode`) antes de alterar `st_abr` e `st_depth`, repassando parâmetros de mistura fidedignos a primitivas não texturizadas (comandos `0x2a` / `0x28`).

## 3. [Validação] Telemetria e F8 Dumps

- [x] 3.1 Validar compilação limpa do executável com o teto de 16 MB preservado (544 KB livres).
- [x] 3.2 Executar os autotestes (`HMBTN_SELFTEST=1`) comprovando passagem em `scenery check`, `bgtrace check`, `album check`, `cache check` e `audio ringbuffer check`.
- [x] 3.3 Executar o comando `git diff -- src/main src/overlay src/include` e confirmar que o resultado é vazio, preservando o isolamento absoluto da HAL.
