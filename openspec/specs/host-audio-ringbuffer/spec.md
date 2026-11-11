# host-audio-ringbuffer Specification

## Purpose
Desacopla a renderização de áudio da SPU do callback síncrono do sistema operacional através de um Ring Buffer circular estático lock-free, garantindo reprodução contínua e imune a buffer underruns causados por picos de carga na CPU.

## Requirements

### Requirement: Lock-Free Asynchronous Decoupling
O callback de áudio do sistema operacional (`SDL_AudioCallback`) SHALL obter amostras de áudio exclusivamente através de leituras no Ring Buffer, sem invocar rotinas de síntese da SPU ou do sequenciador diretamente na thread de reprodução de áudio.

#### Scenario: Audio Playback from Decoupled Buffer
- **WHEN** o hardware de som solicita um bloco de amostras no callback de áudio
- **THEN** o callback consome os dados pré-renderizados do Ring Buffer via cópia contígua de memória
- **AND** a decodificação da SPU permanece executando de forma independente na thread de background

### Requirement: SPSC Pointer Safety and Non-Overlapping
A gestão dos ponteiros de leitura e escrita do Ring Buffer SHALL operar de forma atômica e lock-free, garantindo que o ponteiro de escrita nunca ultrapasse o ponteiro de leitura além da capacidade máxima configurada.

#### Scenario: Continuous High-Throughput Audio Streaming
- **WHEN** a thread produtora e a thread consumidora trocam milhares de amostras em laço contínuo
- **THEN** o índice de escrita permanece maior ou igual ao índice de leitura
- **AND** a distância entre os dois índices nunca ultrapassa a capacidade de 16.384 frames estéreo

### Requirement: Graceful Underrun and Silence Generation
Quando a quantidade de dados disponíveis no Ring Buffer for inferior à quantidade de amostras requisitada pelo callback da placa de som, a HAL SHALL preencher a lacuna com silêncio (amostras zeradas) e incrementar o contador de underruns, sem travar o processo.

#### Scenario: Audio Starvation Recovery
- **WHEN** a thread produtora sofre uma interrupção temporária e o Ring Buffer esgota seus dados
- **THEN** o callback de áudio preenche o buffer da placa de som com zeros
- **AND** o contador de underruns é incrementado
- **AND** a reprodução retoma normalmente assim que a thread produtora volta a encher o buffer

### Requirement: Zero Dynamic Allocation in Audio Pipeline
Todas as estruturas de dados do Ring Buffer e do pipeline de transporte de áudio SHALL residir em memória estática pré-alocada, sendo vedada qualquer invocação de `malloc`, `realloc` ou `calloc` durante o streaming de som.

#### Scenario: Real-Time Audio Streaming Allocation Audit
- **WHEN** o subsistema de áudio executa a geração e consumo contínuo de amostras
- **THEN** nenhuma alocação de memória dinâmica no heap é executada pelo worker ou pelo callback
