#ifndef LIBAPI_H
#define LIBAPI_H

#include <sys/types.h>
#include <kernel.h>

void SetMem(int n);
void SystemError(char c, long n);
long EnterCriticalSection(void);
void ExitCriticalSection(void);
long OpenEvent(unsigned long desc, long spec, long mode, long (*func)(void));
long CloseEvent(long event);
long EnableEvent(long event);
long DisableEvent(long event);
long WaitEvent(long event);
long TestEvent(long event);
long open(char *name, unsigned long mode);
long close(long fd);
long read(long fd, void *buf, long n);
long write(long fd, void *buf, long n);
long lseek(long fd, long offset, long whence);
struct DIRENTRY *firstfile(char *name, struct DIRENTRY *dir);
struct DIRENTRY *nextfile(struct DIRENTRY *dir);
long erase(char *name);
long format(char *fs);
void InitCARD(long val);
long StartCARD(void);
long StopCARD(void);
void _bu_init(void);
long _card_info(long chan);
long _card_load(long chan);
long _card_clear(long chan);
void ChangeClearPAD(long val);
void ChangeClearRCnt(long t, long m);
long StartPAD(void);
void StopPAD(void);
void InitPAD(char *bufA, long lenA, char *bufB, long lenB);
void FlushCache(void);

#endif
