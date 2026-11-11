# Proposal: pc-port-audio-ringbuffer

## Why

Atualmente, o subsistema de áudio executa a interpretação do sequenciador MIDI (`.SEQ`), a decodificação ADPCM de até 24 vozes de hardware da SPU e o loop de interpolação/mixagem sincronicamente dentro do `SDL_AudioCallback` (em `spu_core.c`). O callback de áudio é executado em uma thread de tempo real de altíssima prioridade do sistema operacional. Com a introdução planejada de streaming de áudio CD-XA concorrente e processamento de reverberação/DSP, picos momentâneos de computação excederão a janela temporal do buffer (512 amostras = ~11.6 ms), provocando *buffer underruns*, estalos acústicos (*pops/clicks*) e degradação perceptível.

## What Changes

- **Isolamento do `SDL_AudioCallback`:** O callback de áudio da placa de som torna-se um consumidor ultraleve, executando exclusivamente uma cópia de memória (`memcpy`) de amostras PCM estéreo de 16 bits a partir de um Ring Buffer circular contíguo.
- **Ring Buffer Estático Lock-Free (SPSC):** Alocação de uma estrutura contígua em BSS com capacidade para 16.384 quadros estéreo (64 KB de áudio PCM = ~370 ms de reserva), gerenciada por índices atômicos de leitura e escrita (`SDL_atomic_t`) para garantir thread-safety determinística sem contenção de mutexes (`SDL_LockAudioDevice`).
- **Thread Produtora Dedicada (`audio_worker.c`):** Migração do processamento computacionalmente denso (`spu_render`, `seq_advance`, decodificação VAG e envelopes ADSR) para uma thread de worker em segundo plano (`SDL_CreateThread`), que monitora a capacidade do buffer e gera áudio em lotes antecipados.
- **Tratamento Resiliente de Starvation:** Em caso de esgotamento momentâneo do buffer (underrun), o callback emite silêncio (amostras zeradas), incrementa contadores diagnósticos de telemetria e sobrevive sem travamento ou ruído agressivo.
- **Zero Alocação Dinâmica:** Todo o ciclo de vida do produtor, consumidor e buffer circular opera em arrays e tipos estáticos, garantindo zero chamadas a `malloc`/`free`.
- **Isolamento da Base Descompilada:** A lógica original do PlayStation 1 em `src/main/` e `src/overlay/` permanece 100% inalterada. As modificações residem estritamente na HAL do host em `src/pc_port/`.

## Capabilities

### New Capabilities

- `host-audio-ringbuffer`: Abstração de buffer circular lock-free e thread produtora desacoplada para renderização assíncrona de áudio da SPU no host.

### Modified Capabilities

- `host-source-compat`: O ciclo de vida do subsistema de som passa a inicializar e finalizar a thread de worker do host sem alterar a API pública de `spu_audio_open()` e `spu_audio_close()`.

## Impact

- `src/pc_port`: Novos arquivos `src/pc_port/include/audio_ringbuffer.h`, `src/pc_port/include/audio_worker.h`, `src/pc_port/src/audio_ringbuffer.c`, `src/pc_port/src/audio_worker.c` e harness de validação `src/pc_port/src/audio_ringbuffer_check.c`.
- `spu_core.c`: Refatoração de `audio_callback` para ler do ring buffer atômico em vez de invocar `spu_render` diretamente.
- `cmake/host.cmake`: Inclusão dos novos módulos de áudio no executável `hmbtn-host`.
- Código original do jogo (`src/main/sound/`, `src/include/`): Zero modificações (`git diff` nulo).
- Limitações e Escopo: A latência adicional introduzida pelo buffer de pré-renderização é limitada a poucas dezenas de milissegundos, imperceptível para a dinâmica de um jogo de simulação e ajustável via tamanho de lote.
