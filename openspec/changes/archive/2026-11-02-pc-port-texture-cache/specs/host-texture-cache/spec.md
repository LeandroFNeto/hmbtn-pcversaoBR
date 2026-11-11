# Capability: host-texture-cache

## Purpose

Gerencia o ciclo de vida e a alocação de handles de textura OpenGL para assets de alta definição importados pelo mod loader, limitando o consumo de memória de vídeo e eliminando transferências redundantes de dados via barramento PCI-Express.

## ADDED Requirements

### Requirement: Single Upload per Active Asset
Quando uma textura de substituição for requisitada múltiplas vezes por uma ou mais primitivas gráficas, a HAL SHALL realizar o upload do arquivo para a memória de vídeo do OpenGL exatamente uma vez enquanto o handle permanecer válido no cache.

#### Scenario: Repetitive Draw of the Same Asset
- **WHEN** uma primitiva requisita o asset `A` 100 vezes consecutivas através de `texture_resolve`
- **THEN** a GPU recebe o comando de upload de textura exatamente 1 vez
- **AND** as 99 chamadas subsequentes retornam o identificador `GLuint` em tempo $O(1)$ sem acesso a disco

### Requirement: Fixed Capacity Constraint
O número de texturas ativas mantidas concorrentemente na GPU pelo subsistema de mods SHALL NÃO exceder o limite estático configurado `TEXTURE_CACHE_CAP` (256 slots por padrão).

#### Scenario: Capacity Boundary Enforcement
- **WHEN** 256 texturas distintas são carregadas no cache
- **AND** uma 257ª textura distinta é requisitada
- **THEN** a quantidade total de texturas ativas permanece exatamente em 256

### Requirement: Least Recently Used (LRU) Eviction
Quando o cache atingir a capacidade máxima e um novo asset for requisitado, o gerenciador de cache SHALL sumariamente descarregar da GPU o recurso que não é renderizado há mais tempo através de `glDeleteTextures`.

#### Scenario: Eviction of the Oldest Unreferenced Texture
- **WHEN** o cache está cheio com os assets $T_1$ a $T_{256}$ carregados na ordem de seus índices
- **AND** os assets $T_2$ a $T_{256}$ são acessados novamente no quadro atual
- **AND** um novo asset $T_{257}$ é requisitado
- **THEN** o asset $T_1$ é descarregado da GPU via `glDeleteTextures`
- **AND** o slot vago é ocupado por $T_{257}$

### Requirement: Zero Dynamic Allocation
A estrutura do cache e todas as rotinas de busca, inserção, reposicionamento LRU e descarte SHALL operar exclusivamente sobre memória estática pré-alocada, sem invocar `malloc`, `realloc` ou `calloc` durante o laço de execução do jogo.

#### Scenario: Main Loop Memory Allocation Ban
- **WHEN** o laço de renderização processa a Ordering Table e executa chamadas a `texture_resolve`
- **THEN** nenhuma alocação dinâmica é registrada no subsistema de heap
