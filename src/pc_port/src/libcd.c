/* libcd on the host CD drive (cd_drive.c). Reads finish inside CdRead, so every wait
 * the game polls reports completion at once. */

#include <libcd.h>
#include <cd_drive.h>

#include <SDL.h>

#include <stddef.h>
#include <string.h>

#define CD_LEAD_IN 150
#define CD_SECTORS_PER_SECOND 75

static CdlLOC cd_loc;
static int seek_lba;
static int read_status;
static CdlCB read_callback;
static u_char read_result[8];

static u_char to_bcd(int v) {
    return (u_char)(((v / 10) << 4) | (v % 10));
}

static int from_bcd(u_char v) {
    return (v >> 4) * 10 + (v & 0x0F);
}

CdlLOC *CdIntToPos(int i, CdlLOC *p) {
    if (p == NULL) {
        p = &cd_loc;
    }
    i += CD_LEAD_IN;
    p->minute = to_bcd(i / (CD_SECTORS_PER_SECOND * 60));
    p->second = to_bcd((i / CD_SECTORS_PER_SECOND) % 60);
    p->sector = to_bcd(i % CD_SECTORS_PER_SECOND);
    p->track = 0;
    return p;
}

int CdPosToInt(CdlLOC *p) {
    return (from_bcd(p->minute) * 60 + from_bcd(p->second)) * CD_SECTORS_PER_SECOND + from_bcd(p->sector) -
           CD_LEAD_IN;
}

int CdInit(void) {
    cd_drive_open(NULL);
    return 1;
}

static int control(u_char com, u_char *param) {
    if (com == CdlSetloc && param != NULL) {
        seek_lba = CdPosToInt((CdlLOC *)param);
    }
    return 1;
}

int CdControl(u_char com, u_char *param, u_char *result) {
    (void)result;
    return control(com, param);
}

int CdControlB(u_char com, u_char *param, u_char *result) {
    (void)result;
    return control(com, param);
}

int CdControlF(u_char com, u_char *param) {
    return control(com, param);
}

int CdSync(int mode, u_char *result) {
    (void)mode;
    (void)result;
    return CdlComplete;
}

int CdRead(int sectors, u_long *buf, int mode) {
    (void)mode;
    if (!cd_drive_is_open() || sectors <= 0) {
        return 0;
    }
    read_status = cd_drive_read((uint32_t)seek_lba, (uint32_t)sectors, buf);
    seek_lba += sectors;
    if (read_callback != NULL) {
        read_callback(CdlComplete, read_result);
    }
    return 1;
}

int CdReadSync(int mode, u_char *result) {
    int status;

    (void)mode;
    (void)result;
    status = read_status;
    read_status = 0;
    return status;
}

CdlCB CdReadCallback(CdlCB func) {
    CdlCB previous;

    previous = read_callback;
    read_callback = func;
    return previous;
}

void CdReadBreak(void) {
}

void CdFlush(void) {
}

int CdLastCom(void) {
    return 0;
}

CdlFILE *CdSearchFile(CdlFILE *fp, char *name) {
    uint32_t lba;
    uint32_t size;
    const char *base;

    if (cd_drive_find(name, &lba, &size) != 0) {
        SDL_Log("CdSearchFile: %s not found", name != NULL ? name : "(null)");
        return NULL;
    }
    CdIntToPos((int)lba, &fp->pos);
    fp->size = size;
    base = strrchr(name, '\\');
    base = base != NULL ? base + 1 : name;
    memset(fp->name, 0, sizeof(fp->name));
    strncpy(fp->name, base, sizeof(fp->name) - 1);
    return fp;
}

int cd_debug_seek_lba(void) {
    return seek_lba;
}
