# Design: pc-port-texture-cache

## Context

Atualmente, `mods.c` realiza uma varredura inicial e armazena até 32 slots de modificação de textura estáticos (`slots[32]` e `models[32]`), subindo todos os arquivos encontrados no diretório de mods diretamente para o OpenGL durante a inicialização via `upload_bmp`. Quando o jogo ou um pacote de texturas de alta fidelidade inclui centenas ou milhares de texturas (cenários pré-renderizados substituídos, variações climáticas, modelos adicionais via `tmd_id`), essa abordagem inicial gera dois problemas críticos:
1. Tempo de inicialização proibitivo e desperdício de memória de GPU com assets não visíveis na cena atual.
2. Inviabilidade de escala: pacotes 4K com mais de 32 arquivos estouram o limite estático rígido ou esgotam o heap de vídeo da máquina hospedeira sem política de reciclagem.

O gerenciador de cache LRU resolve esses problemas criando uma piscina estática de texturas com carregamento sob demanda (lazy loading) e descarte determinístico.

## Goals / Non-Goals

**Goals:**
- Prover um cache LRU estático com capacidade fixa de `TEXTURE_CACHE_CAP` (256 entradas por padrão, com suporte compilado a até 512).
- Garantir tempo $O(1)$ tanto para verificação de hit quanto para descarte e inserção de novos elementos.
- Garantir alocação estática absoluta: zero chamadas a `malloc`, `calloc` ou `free` no caminho crítico de renderização.
- Descarregar da GPU (`glDeleteTextures`) a textura menos recentemente acessada assim que a capacidade limite for atingida.
- Assegurar que múltiplos quadros renderizando o mesmo asset realizem exatamente 1 upload para a GPU enquanto o asset residir no cache.
- Manter o isolamento estrito de `src/main/` e `src/overlay/`.

**Non-Goals:**
- Streaming assíncrono em background thread (a decodificação e upload ocorrem no momento da primeira demanda na frame).
- Compressão de texturas de vídeo (DXT/BC) proprietária.
- Alteração no parser de geometria original da engine do PS1.

## Architecture & Data Structures

Para atingir tempo $O(1)$ sem alocação dinâmica, o gerenciador utiliza duas estruturas de dados indexadas por inteiros dentro de arrays contíguos pré-alocados em BSS:

### 1. Tabela Hash com Open Addressing (Sondagem Linear)
Um array de inteiros estático `hash_table[HASH_CAP]` armazena índices para a tabela de entradas.
- O hash é calculado a partir de um valor de 64 bits composto por:
  - Tipo de chave (Página/UV vs TMD ID)
  - Identificador único da textura (hash do caminho do arquivo ou tupla `page, uv` / `tmd_id`).
- Sondagem linear resolve colisões sem necessidade de ponteiros ou listas dinâmicas.

### 2. Lista Duplamente Encadeada Estática (LRU Chain)
Cada slot de textura contém ponteiros de índice inteiro (`prev` e `next`), eliminando alocação de nós:

```c
#define TEXTURE_CACHE_CAP 256
#define TEXTURE_HASH_CAP  512

typedef struct TextureCacheEntry {
    uint64_t key;            /* Identificador unívoco do asset */
    GLuint gl_name;          /* Handle nativo de textura OpenGL */
    char path[260];          /* Caminho do arquivo para recarga sob demanda */
    uint32_t last_frame;     /* Número do quadro do último acesso */
    uint16_t prev;           /* Índice do nó anterior na fila LRU */
    uint16_t next;           /* Índice do próximo nó na fila LRU */
    uint8_t in_use;          /* Flag indicando slot ocupado */
} TextureCacheEntry;

typedef struct TextureCache {
    TextureCacheEntry entries[TEXTURE_CACHE_CAP];
    uint16_t hash_table[TEXTURE_HASH_CAP];
    uint16_t lru_head;       /* Most Recently Used (MRU) */
    uint16_t lru_tail;       /* Least Recently Used (LRU) */
    uint16_t count;
    uint32_t current_frame;
    /* Métricas de telemetria */
    uint32_t stat_hits;
    uint32_t stat_misses;
    uint32_t stat_uploads;
    uint32_t stat_evictions;
} TextureCache;
```

## Decisions

1. **Lazy Loading sob demanda:** Em vez de fazer upload de todos os BMPs/PNGs durante o scan inicial em `mods_rescan`, os arquivos são apenas catalogados em um índice leve de caminhos. A criação do handle OpenGL via `upload_bmp` ocorre estritamente na primeira chamada a `texture_cache_acquire` que resulte em miss.
2. **Atualização O(1) de Recência:** Cada hit no cache desvincula a entrada de sua posição atual na lista encadeada e a reposiciona no topo (`lru_head`). A entrada na cauda (`lru_tail`) é sempre o alvo imediato de descarte (eviction).
3. **Descarte Imediato no Driver:** Ao atingir `TEXTURE_CACHE_CAP`, `glDeleteTextures(1, &entry->gl_name)` é invocado imediatamente antes de reutilizar o slot. A entrada é desvinculada da tabela hash e reinicializada.
4. **Proteção de Texturas no Mesmo Quadro:** Para evitar que uma cena complexa com mais de 256 texturas descarte uma textura utilizada no início do próprio quadro em renderização, o descarte verifica se `entry->last_frame == current_frame`. Se todas as texturas forem do quadro corrente, o cache registra um alerta de saturação e prioriza a entrada mais antiga do frame.
5. **Autoteste Determinístico:** O módulo de validação `texture_cache_check.c` cria texturas sintéticas temporárias para comprovar:
   - 100 requisições do mesmo asset geram exatamente 1 upload para a GPU (99 hits).
   - Inserção de 257 assets distintos causa exatamente 1 descarte (`stat_evictions == 1`) e mantém o `count` estritamente em 256.
   - O elemento mais antigo não acessado é o primeiro a ser desalocado da memória de vídeo.

## Risks / Trade-offs

- **[Pico de Frame Time no Primeiro Miss]** → Carregar e subir uma textura 4K no meio da renderização pode introduzir micro-stuttering no primeiro frame em que o objeto surge.
  *Mitigação:* Como o jogo possui transições de mapa e portas (`scene_enter`), o cache expõe `texture_cache_prefetch()` para aquecimento prévio dos assets mapeados para a área atual.
- **[Conflito de Hash]** → Sondagem linear em tabela com fator de carga próximo a 50% garante busca de baixíssimo ciclo e sem travamentos.
- **[Fuga de VRAM em Hardware Antigo]** → A capacidade padrão de 256 texturas com teto de resolução protege mesmo placas de vídeo integradas com recursos modestos.

## Migration Plan

- Substituição limpa do array fixo atual de `slots` em `mods.c`. O retorno de `texture_resolve` e as assinaturas públicas permanecem idênticos.
- O rollback é imediato, bastando reverter para os commits anteriores em `src/pc_port/`.
- Compatibilidade reversa de saves e binários originais permanece 100% inalterada.
