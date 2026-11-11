# Tasks: pc-port-audio-ringbuffer

## 1. Static Ring Buffer Engine & SPSC Core

- [x] 1.1 Definir `src/pc_port/include/audio_ringbuffer.h` com a interface lock-free: `audio_ringbuffer_init()`, `audio_ringbuffer_write()`, `audio_ringbuffer_read()`, `audio_ringbuffer_available_read()`, `audio_ringbuffer_available_write()` e métricas atômicas associadas.
- [x] 1.2 Implementar `src/pc_port/src/audio_ringbuffer.c` com capacidade fixa de `AUDIO_RING_FRAMES` (16.384 frames estéreo = 64 KB em BSS), índices atômicos de leitura/escrita via `SDL_AtomicGet`/`SDL_AtomicSet` e operações de wrap circular com máscara binária.
- [x] 1.3 Implementar a lógica de cópia segura com preenchimento de silêncio (`memset` de zeros) e incremento de `underrun_count` em cenários de dados parciais ou ausentes.

## 2. Audio Worker Thread Implementation

- [x] 2.1 Definir `src/pc_port/include/audio_worker.h` com as funções de ciclo de vida: `audio_worker_start()`, `audio_worker_stop()`, `audio_worker_pause()` e controle de estado da thread.
- [x] 2.2 Implementar `src/pc_port/src/audio_worker.c` utilizando `SDL_CreateThread`, com loop de monitoramento de nível de buffer, geração de lotes de 512 frames via `spu_render()` e suspensão adaptativa (`SDL_Delay(2)`) ao atingir 75% da capacidade.
- [x] 2.3 Garantir finalização limpa e determinística do worker thread em `audio_worker_stop()` utilizando `SDL_WaitThread`.

## 3. Host HAL Integration & Callback Refactoring

- [x] 3.1 Refatorar `audio_callback` em `src/pc_port/src/spu_core.c` para operar estritamente como consumidor, substituindo a chamada direta a `spu_render` por `audio_ringbuffer_read()`.
- [x] 3.2 Atualizar `spu_audio_open()`, `spu_audio_pause()` e `spu_audio_close()` em `src/pc_port/src/spu_core.c` para inicializar o ring buffer e gerenciar o ciclo de vida do audio worker.
- [x] 3.3 Atualizar `cmake/host.cmake` para incluir os novos arquivos de áudio do host na compilação de `hmbtn-host`.

## 4. Automated Stress Verification & Selftests

- [x] 4.1 Implementar `src/pc_port/src/audio_ringbuffer_check.c` e integrá-lo ao modo `HMBTN_SELFTEST=1`:
  - **Cenário de Estresse de Concorrência (Memory Safety):** Executar transferência contínua de 100.000 amostras entre produtor e consumidor sintéticos, verificando que os índices atômicos nunca colidem ou violam os limites do buffer circular.
  - **Cenário de Sobrevivência a Starvation (Callback Survival):** Injetar pausa forçada na thread produtora (simulando atraso de CD-XA), disparar o callback sucessivas vezes e verificar a emissão estrita de silêncio, a contagem de underruns e a ausência de travamento.
  - **Auditoria de Memória:** Comprovar ausência de chamadas `malloc`/`free` em todo o ciclo de transporte de áudio.
- [x] 4.2 Confirmar que `git diff -- src/main src/overlay src/include` permanece estritamente vazio.
- [x] 4.3 Confirmar que o layout final do binário `hmbtn-host.exe` permanece estritamente abaixo do limite de 16 MB (`< 0x1000000`).

```
audio ringbuffer check ok: 100,000 samples transferred without index collision or corruption
audio ringbuffer check ok: zero underruns during sustained balanced transfer
audio ringbuffer check ok: empty buffer increments underrun counter
audio ringbuffer check ok: callback outputs pure silence under starvation
audio ringbuffer check ok: partial read consumed exact available frames
audio ringbuffer check ok: partial read triggered underrun count
audio ringbuffer check ok: partial starvation padded missing tail with silence
audio ringbuffer check ok: audio worker thread started successfully
audio ringbuffer check ok: audio worker produces sound into ring buffer asynchronously
audio ringbuffer check ok: consumer survives worker pause via graceful starvation
audio ringbuffer check ok: worker recovered and resumed audio production after pause
audio ringbuffer check ok: audio worker stopped cleanly via SDL_WaitThread
audio ringbuffer check ok: zero runtime heap allocations in ringbuffer and worker pipeline (pure BSS)
```
