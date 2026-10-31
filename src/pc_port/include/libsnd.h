#ifndef LIBSND_H
#define LIBSND_H

#include <sys/types.h>

#define SS_SEQ_TABSIZ 176
#define SS_NOTICK 0x1000
#define SS_NOTICK0 0
#define SS_TICK60 1
#define SS_TICK240 2
#define SS_TICK120 3
#define SS_TICK50 4
#define SS_TICKVSYNC 5
#define SS_TICKMODE_MAX 6
#define SSPLAY_INFINITY 0
#define SS_SOFT 0
#define SS_HARD 1
#define SS_REV_TYPE_OFF 0
#define SS_REV_TYPE_ROOM 1
#define SS_REV_TYPE_STUDIO_A 2
#define SS_REV_TYPE_STUDIO_B 3
#define SS_REV_TYPE_STUDIO_C 4
#define SS_REV_TYPE_HALL 5
#define SS_REV_TYPE_SPACE 6
#define SS_REV_TYPE_ECHO 7
#define SS_REV_TYPE_DELAY 8
#define SS_REV_TYPE_PIPE 9
#define SS_SERIAL_A 0
#define SS_SERIAL_B 1
#define SS_MIX 0
#define SS_REV 1
#define SS_WAIT_COMPLETED 1

typedef struct VabHdr {
    long form;
    long ver;
    long id;
    u_long fsize;
    u_short reserved0;
    u_short ps;
    u_short ts;
    u_short vs;
    u_char mvol;
    u_char pan;
    u_char attr1;
    u_char attr2;
    u_long reserved1;
} VabHdr;

typedef struct ProgAtr {
    u_char tones;
    u_char mvol;
    u_char prior;
    u_char mode;
    u_char mpan;
    char reserved0;
    short attr;
    u_long reserved1;
    u_long reserved2;
} ProgAtr;

typedef struct VagAtr {
    u_char prior;
    u_char mode;
    u_char vol;
    u_char pan;
    u_char center;
    u_char shift;
    u_char min;
    u_char max;
    u_char vibW;
    u_char vibT;
    u_char porW;
    u_char porT;
    u_char pbmin;
    u_char pbmax;
    u_char reserved1;
    u_char reserved2;
    u_short adsr1;
    u_short adsr2;
    short prog;
    short vag;
    short reserved[4];
} VagAtr;

typedef struct {
    short left;
    short right;
} SndVolume;

void SsInit(void);
void SsEnd(void);
void SsQuit(void);
void SsStart(void);
void SsStart2(void);
void SsSetTickMode(long mode);
void SsSetTableSize(char *table, short s_max, short t_max);
void SsSetMVol(short voll, short volr);
void SsSetSerialAttr(char s_num, char attr, char mode);
void SsSetSerialVol(char s_num, short voll, short volr);
short SsVabOpenHead(u_char *addr, short vabid);
short SsVabTransBody(u_char *addr, short vabid);
short SsVabTransBodyPartly(u_char *addr, u_long bufsize, short vabid);
short SsVabTransCompleted(short immediateFlag);
void SsVabClose(short vabid);
short SsUtKeyOn(short vabId, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOff(short voice, short vabId, short prog, short tone, short note);
short SsUtKeyOnV(short voice, short vabId, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOffV(short voice);
short SsUtSetVVol(short vc, short voll, short volr);
short SsUtGetVVol(short vc, short *voll, short *volr);
short SsUtChangePitch(short voice, short vabId, short prog, short old_note, short old_fine, short new_note, short new_fine);
short SsUtGetProgAtr(short vabId, short progNum, ProgAtr *progatrptr);
short SsUtGetVagAtr(short vabId, short progNum, short toneNum, VagAtr *vagatrptr);
void SsUtAllKeyOff(short mode);
void SsUtReverbOn(void);
void SsUtReverbOff(void);
short SsUtSetReverbType(short type);
void SsUtSetReverbDepth(short ldepth, short rdepth);
char SsSetReservedVoice(char voices);
short SsSeqOpen(u_long *addr, short vab_id);
void SsSeqClose(short seq_access_num);
void SsSeqPlay(short seq_access_num, char play_mode, short l_count);
void SsSeqPause(short seq_access_num);
void SsSeqReplay(short seq_access_num);
void SsSeqStop(short seq_access_num);
void SsSeqSetVol(short seq_access_num, short voll, short volr);

#endif
