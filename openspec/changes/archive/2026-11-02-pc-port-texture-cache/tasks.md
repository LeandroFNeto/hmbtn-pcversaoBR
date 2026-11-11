# Tasks: pc-port-texture-cache

## 1. Core Cache Engine & Static Data Structures

- [x] 1.1 Definir `src/pc_port/include/texture_cache.h` com a interface do gerenciador: `texture_cache_init()`, `texture_cache_shutdown()`, `texture_cache_acquire(uint64_t key, const char *path)`, `texture_cache_tick(uint32_t frame_num)` e estruturas estáticas associadas.
- [x] 1.2 Implementar `src/pc_port/src/texture_cache.c` com capacidade fixa de `TEXTURE_CACHE_CAP` (256 slots estáticos), tabela hash com sondagem linear de resolução rápida e lista duplamente encadeada com índices estáticos (`prev`/`next`) para ordenação LRU em $O(1)$.
- [x] 1.3 Implementar a rotina de descarte atômico (eviction): identificação do `lru_tail`, invocação de `glDeleteTextures(1, &entry->gl_name)`, desvinculação da tabela hash e reinicialização segura do slot sem fragmentação.

## 2. Integration with Host HAL & Mod Loader

- [x] 2.1 Refatorar `src/pc_port/src/mods.c` para registrar o catálogo de caminhos em vez de efetuar upload massivo no scan inicial (`mods_rescan`).
- [x] 2.2 Integrar a busca de `mods_lookup` e `mods_lookup_model` ao `texture_cache_acquire`, delegando o upload da textura apenas no instante do primeiro *cache miss*.
- [x] 2.3 Conectar o avanço de quadros do host (`main_pc.c` / `libetc.c`) ao `texture_cache_tick` para registrar a timestamp/número do frame corrente em cada acesso.
- [x] 2.4 Atualizar `cmake/host.cmake` para incluir `src/pc_port/src/texture_cache.c` e `src/pc_port/src/texture_cache_check.c` na compilação do executável `hmbtn-host`.

## 3. Automated Verification & Selftests

- [x] 3.1 Implementar `src/pc_port/src/texture_cache_check.c` e integrá-lo ao modo `HMBTN_SELFTEST=1`:
  - Validar garantia de upload único: 100 requisições consecutivas da mesma chave geram exatamente 1 chamada ao driver de GPU e 99 hits.
  - Validar retenção e capacidade limite: carregar 256 texturas distintas e verificar `stat_evictions == 0` e `count == 256`.
  - Validar descarte estrito LRU: solicitar uma 257ª textura e comprovar que o elemento mais antigo foi descarregado da GPU (`stat_evictions == 1`) e que o seu handle foi deletado via `glDeleteTextures`.
  - Validar priorização de acesso: acessar o elemento mais antigo antes da inserção excedente, garantindo que o segundo elemento mais antigo seja o descartado.
- [x] 3.2 Verificar ausência absoluta de chamadas `malloc`/`free` durante as operações de consulta e descarte no loop de selftest.

```
cache check ok: first acquire returns valid handle
cache check ok: 100 queries generated 1 upload and 99 hits
cache check ok: zero evictions during hits
cache check ok: exactly 1 active texture in cache
cache check ok: cache filled to exactly 256 capacity
cache check ok: 256 distinct uploads performed
cache check ok: zero evictions before exceeding capacity
cache check ok: 257th asset acquired successfully
cache check ok: active count remains bounded at 256
cache check ok: exactly 1 eviction triggered upon reaching 257 assets
cache check ok: evicted key triggered re-upload on subsequent access
cache check ok: cache cleared successfully
cache check ok: one eviction occurred on overflow
cache check ok: accessed key 1 was preserved as MRU
cache check ok: second oldest key 2 was the evicted victim
cache check ok: zero runtime heap allocations in cache path (all arrays static)
```

## 4. Isolation Check & Documentation

- [x] 4.1 Confirmar que `git diff -- src/main src/overlay src/include` permanece estritamente vazio.
- [x] 4.2 Documentar o gerenciador e métricas de cache na arquitetura da HAL.
