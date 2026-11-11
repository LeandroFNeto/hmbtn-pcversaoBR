# Design: pc-port-audio-ringbuffer

## Context

No design legado da HAL em [src/pc_port/src/spu_core.c](file:///c:/Projeto-futuro-junior/hmbtn-decomp/src/pc_port/src/spu_core.c), a função `audio_callback` invocada pela thread de áudio do SDL executa diretamente `spu_render((int16_t *)stream, frames)`. Essa chamada cascateia:
1. Processamento e avanço de eventos de tempo da sequência (`seq_run_due_events`, `seq_advance`).
2. Interpolação polinomial e decodificação ADPCM bloco a bloco em até 24 canais da SPU.
3. Síntese de volumes e envelopes de amplitude ADSR.
4. Acumulação e saturação estéreo em `mix_acc`.

Esse modelo acopla o tempo de computação da CPU à taxa estrita de consumo do hardware de som. Picos de processamento resultam em estouro da janela de entrega do callback, provocando estalos sonoros e travamentos. O desacoplamento assíncrono via Ring Buffer SPSC (*Single-Producer Single-Consumer*) resolve esse ponto único de falha.

## Goals / Non-Goals

**Goals:**
- Desacoplar a thread de renderização da SPU da thread de entrega de áudio do sistema operacional.
- Tornar o `SDL_AudioCallback` ultraleve (somente `memcpy` e preenchimento de silêncio sob underrun).
- Garantir concorrência lock-free entre a thread produtora e a consumidora utilizando operações atômicas da SDL (`SDL_AtomicGet` / `SDL_AtomicSet`).
- Operar exclusivamente sobre memória estática em BSS (zero chamadas a `malloc`/`free` no caminho de áudio).
- Proteger contra estouro de ponteiros e descontinuidade temporal em cenários de sobrecarga.
- Manter o executável do host estritamente abaixo do teto de 16 MB (`< 0x1000000`).

**Non-Goals:**
- Implementação da decodificação de setores CD-XA reais (escopo de fases subsequentes).
- Algoritmos avançados de re-amostragem (resampling) dinâmica com pitch-shifting de fração de hertz.

## Architecture & Concurrency Model

```
+-----------------------------------------------------------------------------------+
|                        ASYNC AUDIO PIPELINE (SPSC LOCK-FREE)                      |
|                                                                                   |
|  [ Audio Worker Thread ]                      [ SDL_AudioCallback Thread ]        |
|  (Produtor em Background)                     (Consumidor OS em Tempo Real)       |
|                                                                                   |
|        |                                                   ^                      |
|   spu_render()                                             |                      |
|   seq_advance()                                         memcpy()                  |
|        |                                                   |                      |
|        v                                                   |                      |
|  +---------------------------------------------------------------+                |
|  |             STATIC RING BUFFER (16,384 stereo frames, 64 KB)  |                |
|  +---------------------------------------------------------------+                |
|       ^                                                   ^                       |
|       |                                                   |                       |
|  write_pos (Atomic)                              read_pos (Atomic)                |
+-----------------------------------------------------------------------------------+
```

### 1. Estruturas Estáticas Contíguas
O buffer é dimensionado como uma potência de 2 para permitir avanço de índice circular via máscara binária rápida (`index & MASK`):

```c
#define AUDIO_RING_FRAMES 16384        /* 16,384 frames estéreo de 16-bit */
#define AUDIO_RING_MASK   (AUDIO_RING_FRAMES - 1)
#define AUDIO_CHUNK_BATCH 512          /* Tamanho do lote de geração */

typedef struct AudioRingBuffer {
    int16_t buffer[AUDIO_RING_FRAMES * 2]; /* 64 KB */
    SDL_atomic_t write_pos;                /* Avançado apenas pelo Worker */
    SDL_atomic_t read_pos;                 /* Avançado apenas pelo Callback */
    SDL_atomic_t underrun_count;           /* Contador diagnóstico */
} AudioRingBuffer;
```

### 2. Sincronização SPSC (Single-Producer Single-Consumer)
- **Leitura do Produtor:** O worker consulta `SDL_AtomicGet(&rb->read_pos)` para calcular o espaço livre:
  $$\text{free\_frames} = \text{AUDIO\_RING\_FRAMES} - (\text{write\_pos} - \text{read\_pos})$$
  Se $\text{free\_frames} \ge \text{AUDIO\_CHUNK\_BATCH}$, gera um lote via `spu_render` e atualiza `write_pos` com `SDL_AtomicSet`.
- **Leitura do Consumidor:** O callback consulta `SDL_AtomicGet(&rb->write_pos)` para calcular as amostras prontas:
  $$\text{ready\_frames} = \text{write\_pos} - \text{read\_pos}$$
  Copia os dados necessários para o stream de áudio e atualiza `read_pos` com `SDL_AtomicSet`.

## Decisions

1. **Capacidade Fixa de 16.384 Amostras (64 KB):** A 44.100 Hz, 16.384 quadros representam ~371 ms de áudio armazenado. Esse valor absorve com ampla margem qualquer pausa causada por carga de disco ou garbage collection de driver gráfico sem introduzir consumo excessivo de BSS (ocupando apenas 64 KB do headroom restante de 650 KB).
2. **Pacing e Sono do Worker:** Para evitar consumo indevido de 100% de um núcleo de CPU quando o buffer estiver cheio, o worker adota estratégia adaptativa: quando o buffer atinge 75% da capacidade, executa `SDL_Delay(2)`. Ao cair abaixo de 50%, retoma a produção imediata em lotes de 512 frames.
3. **Resiliência a Starvation (Underrun Seguro):** Quando $\text{ready\_frames} < \text{requested\_frames}$, o callback copia as amostras residuais e preenche o restante da janela com zeros (`memset`), incrementando atomicamente `underrun_count`. A placa de som recebe silêncio em vez de ruído estático não inicializado ou congelamento de processo.
4. **Encerramento Determinístico:** Um flag atômico `worker_running` é monitorado pela thread. Em `spu_audio_close()`, o flag é zerado e `SDL_WaitThread` aguarda a finalização limpa antes de fechar o dispositivo de áudio.
5. **Autotestes de Estresse (`HMBTN_SELFTEST=1`):**
   - *Segurança de Ponteiros:* Teste sintético de alta velocidade com 100.000 amostras transferidas sem perdas, garantindo que $\text{write\_pos} \ge \text{read\_pos}$ e que a distância nunca exceda a capacidade máxima.
   - *Injeção de Latência (Starvation):* Injeção de bloqueio simulado de 50 ms na thread do produtor enquanto o consumidor requisita áudio, comprovando a geração de silêncio e a retomada perfeita sem crashes.

## Risks / Trade-offs

- **[Latência de Feedback Acústico]** → Um buffer de 370 ms preenchido pode retardar sutilmente efeitos sonoros imediatos disparados por comandos de UI.
  *Mitigação:* O worker mantém o nível alvo dinâmico ajustado entre 2.048 e 4.096 frames (~46 a 92 ms de latência real), mantendo o restante dos 16.384 frames como reserva para amortecimento de picos extremos.
- **[Concorrência na Seção Crítica da SPU]** → Comandos de hardware do PS1 (`SpuSetVoiceAttr`, `SpuKeyOn`) chegam pela thread do jogo, enquanto a síntese roda no worker.
  *Mitigação:* O acesso a estruturas de vozes mantém a sincronização protegida via lock leve interno compartilhado durante atualizações de parâmetros.

## Migration Plan

- Substituição modular no `spu_core.c` isolando o callback de áudio.
- Zero impacto no código do jogo descompilado.
- Reversão simples desfazendo os commits da pasta `src/pc_port/`.
