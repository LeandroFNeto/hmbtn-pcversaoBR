#ifndef AUDIO_WORKER_H
#define AUDIO_WORKER_H

void audio_worker_start(void);
void audio_worker_stop(void);
void audio_worker_pause(int paused);
int audio_worker_is_running(void);

/* Test hook: injects artificial latency in the worker loop to simulate CD-XA or heavy DSP */
void audio_worker_inject_pause(int ms);

#endif /* AUDIO_WORKER_H */
