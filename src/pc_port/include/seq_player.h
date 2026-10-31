#ifndef PC_PORT_SEQ_PLAYER_H
#define PC_PORT_SEQ_PLAYER_H

#include <stdint.h>

/* The host SEQ player: PS1 SEQ streams read on the audio clock. The audio callback
 * splits each buffer at seq_frames_until_next_event, runs due events, and mixes. */

#define SEQ_SLOTS 8
#define SEQ_SLOT_SIZE (64 * 1024)
#define SEQ_NO_EVENT 0x7FFFFFFF

/* Audio-thread side, called with the device lock held. */
int seq_frames_until_next_event(void);
void seq_run_due_events(void);
void seq_advance(int frames);

/* A sound effect keyed on `voice`; the music no longer owns it. */
void seq_voice_taken(int voice);

/* libsnd.c helpers the sequencer uses. */
short snd_key_on_ex(short voice, short vabId, short prog, short tone, short note, short fine, short voll,
                    short volr, int pan_offset, int *out_l, int *out_r);
int snd_pick_voice(void);
int snd_program_tones(short vabId, short prog);
int snd_tone_range(short vabId, short prog, short tone, int *min, int *max);

/* Probes, for checks. */
int seq_debug_resolution(short handle);
uint32_t seq_debug_tempo(short handle);
uint32_t seq_debug_note_ons(short handle);

#endif
