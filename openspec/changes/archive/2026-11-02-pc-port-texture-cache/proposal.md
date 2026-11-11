# Proposal: pc-port-texture-cache

## Why

A injeção de texturas de alta definição via `texture_resolve` e o carregamento baseado em `tmd_id` e coordenadas de VRAM operam sem controle de retenção na memória de vídeo. Sem um mecanismo de cache com limite superior, pacotes extensos de modificação gráfica (ex: substituições 2K/4K ou substituição massiva de cenários e malhas) carregam centenas de texturas na GPU sem descarte, gerando risco iminente de saturação do barramento PCI-Express, consumo descontrolado de VRAM da placa de vídeo hospedeira e quedas drásticas de framerate decorrentes de paginação de driver.

## What Changes

- **Cache LRU Estático de Tamanho Fixo:** Introdução de um subsistema de gerenciamento de texturas na HAL (`src/pc_port/src/texture_cache.c` e `texture_cache.h`) com capacidade estaticamente configurada (256 slots por padrão, expansível a 512), garantindo zero chamadas a `malloc`/`free` durante o loop principal.
- **Indexação por Hash e Consulta O(1):** Associação unívoca entre o identificador do asset (combinação de `page`/`UV` ou `tmd_id` / caminho do arquivo) e o identificador nativo OpenGL (`GLuint`), eliminando leituras de disco e uploads redundantes para a GPU após a primeira carga.
- **Política de Descarte Determinística (Eviction):** Ao atingir a capacidade máxima, a entrada menos recentemente utilizada (LRU) é descarregada da GPU via `glDeleteTextures(1, &tex_id)` e seu slot é reaproveitado para o novo asset sob demanda.
- **Métricas e Diagnóstico Host:** Contadores estáticos de *cache hits*, *cache misses*, *uploads* e *evictions*, consultáveis pelo sistema de telemetria e pelos testes automáticos (`HMBTN_SELFTEST=1`).
- **Isolamento Absoluto:** Nenhuma linha dos diretórios originais `src/main/` ou `src/overlay/` é tocada; todo o ciclo de vida do cache reside na camada de compatibilidade do host.

## Capabilities

### New Capabilities

- `host-texture-cache`: Gerenciamento estático de ciclo de vida de texturas OpenGL para mods do host com substituição LRU e garantia de upload único por asset ativo.

### Modified Capabilities

- `host-texture-mods`: As rotinas de consulta (`mods_lookup`, `mods_lookup_model` e `texture_resolve`) passam a resolver handles através do gerenciador de cache LRU em vez de manter texturas permanentemente fixas em memória.

## Impact

- `src/pc_port`: Novos arquivos `src/pc_port/include/texture_cache.h` e `src/pc_port/src/texture_cache.c`; integração com `mods.c`, `vram.c` e `libgpu.c`; extensão do autoteste em `src/pc_port/src/texture_cache_check.c`.
- `cmake/host.cmake`: Inclusão dos novos fontes do host no alvo executável `hmbtn-host`.
- Código Descompilado Original: Zero impacto. `git diff -- src/main src/overlay src/include` permanece estritamente vazio.
- Limitações e Escopo: O cache gerencia o ciclo de vida de texturas carregadas para a GPU hospedeira; a decodificação de imagem permanece síncrona sob demanda no primeiro acesso da cena.
