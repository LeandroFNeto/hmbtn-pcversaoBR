/* libmcrd over the virtual card in memcard.c. On the PS1 the file commands run in the
 * background and MemCardSync reports them later; here each one finishes inside its call,
 * and its (command, result) pair waits for the next MemCardSync, which reports it once.
 * Channel 0x00 is slot 1 (the card); every other channel has no card. */

#include <libmcrd.h>
#include <host.h>
#include <memcard.h>

#include <SDL.h>

#include <string.h>

#define SLOT1 0x00

static int pending;
static long pending_cmd;
static long pending_result;
static MemCB callback;
static char open_file[MEMCARD_NAME_MAX + 1];

static void trace(const char *call, long chan, long result) {
    if (host_tracing()) {
        SDL_Log("trace: vblank %d %s(0x%02lX) -> %ld", host_vblank_count(), call, chan, result);
    }
}

static void finish(long cmd, long result) {
    pending = 1;
    pending_cmd = cmd;
    pending_result = result;
    if (callback != NULL) {
        callback(cmd, result);
    }
}

/* The card's state as Exist (connected) or Accept (connected and formatted) sees it. */
static long state_result(long chan, int accept) {
    if (chan != SLOT1 || !memcard_is_open()) {
        return McErrCardNotExist;
    }
    switch (memcard_state()) {
    case MEMCARD_READY:
        return McErrNone;
    case MEMCARD_UNFORMATTED:
        return accept ? McErrNotFormat : McErrNone;
    case MEMCARD_INVALID:
        return McErrCardInvalid;
    default:
        return McErrCardNotExist;
    }
}

static long file_result(MemcardResult result) {
    switch (result) {
    case MEMCARD_OK:
        return McErrNone;
    case MEMCARD_NOT_FOUND:
        return McErrFileNotExist;
    case MEMCARD_EXISTS:
        return McErrAlreadyExist;
    case MEMCARD_FULL:
        return McErrBlockFull;
    default:
        return McErrCardInvalid;
    }
}

/* McErrNone when the card in `chan` can hold files, otherwise the state result. */
static long usable(long chan) {
    return state_result(chan, 1);
}

void MemCardInit(long val) {
    (void)val;
    if (!memcard_is_open()) {
        memcard_open(memcard_default_path());
    }
    trace("MemCardInit", val, memcard_state());
}

void MemCardEnd(void) {
}

void MemCardStart(void) {
}

void MemCardStop(void) {
}

long MemCardExist(long chan) {
    finish(McFuncExist, state_result(chan, 0));
    trace("MemCardExist", chan, pending_result);
    return 1;
}

long MemCardAccept(long chan) {
    finish(McFuncAccept, state_result(chan, 1));
    trace("MemCardAccept", chan, pending_result);
    return 1;
}

long MemCardReadFile(long chan, char *file, u_long *adrs, long ofs, long bytes) {
    long result;

    result = usable(chan);
    if (result == McErrNone) {
        result = file_result(memcard_read(file, adrs, ofs, bytes));
    }
    finish(McFuncReadFile, result);
    trace("MemCardReadFile", chan, result);
    return McErrNone;
}

long MemCardWriteFile(long chan, char *file, u_long *adrs, long ofs, long bytes) {
    long result;

    result = usable(chan);
    if (result == McErrNone) {
        result = file_result(memcard_write(file, adrs, ofs, bytes));
    }
    finish(McFuncWriteFile, result);
    trace("MemCardWriteFile", chan, result);
    return McErrNone;
}

long MemCardOpen(long chan, char *file, long flag) {
    long result;

    (void)flag;
    open_file[0] = '\0';
    result = usable(chan);
    if (result == McErrNone && memcard_find(file) < 0) {
        result = McErrFileNotExist;
    }
    if (result == McErrNone) {
        strncpy(open_file, file, MEMCARD_NAME_MAX);
        open_file[MEMCARD_NAME_MAX] = '\0';
    }
    trace("MemCardOpen", chan, result);
    return result;
}

void MemCardClose(void) {
    open_file[0] = '\0';
}

long MemCardReadData(u_long *adrs, long ofs, long bytes) {
    long result;

    result = open_file[0] == '\0' ? McErrFileNotExist : file_result(memcard_read(open_file, adrs, ofs, bytes));
    finish(McFuncReadData, result);
    trace("MemCardReadData", SLOT1, result);
    return McErrNone;
}

long MemCardWriteData(u_long *adrs, long ofs, long bytes) {
    long result;

    result = open_file[0] == '\0' ? McErrFileNotExist : file_result(memcard_write(open_file, adrs, ofs, bytes));
    finish(McFuncWriteData, result);
    trace("MemCardWriteData", SLOT1, result);
    return McErrNone;
}

long MemCardCreateFile(long chan, char *file, long blocks) {
    long result;

    result = usable(chan);
    if (result == McErrNone) {
        result = file_result(memcard_create(file, (int)blocks));
    }
    trace("MemCardCreateFile", chan, result);
    return result;
}

long MemCardDeleteFile(long chan, char *file) {
    long result;

    result = usable(chan);
    if (result == McErrNone) {
        result = file_result(memcard_delete(file));
    }
    trace("MemCardDeleteFile", chan, result);
    return result;
}

long MemCardFormat(long chan) {
    long result;

    result = state_result(chan, 0);
    if (result == McErrNone) {
        result = memcard_format() == MEMCARD_OK ? McErrNone : McErrCardInvalid;
    }
    trace("MemCardFormat", chan, result);
    return result;
}

long MemCardUnformat(long chan) {
    long result;

    result = state_result(chan, 0);
    if (result == McErrNone) {
        result = memcard_unformat() == MEMCARD_OK ? McErrNone : McErrCardInvalid;
    }
    trace("MemCardUnformat", chan, result);
    return result;
}

long MemCardSync(long mode, long *cmds, long *rslt) {
    (void)mode;
    if (!pending) {
        return -1;
    }
    pending = 0;
    if (cmds != NULL) {
        *cmds = pending_cmd;
    }
    if (rslt != NULL) {
        *rslt = pending_result;
    }
    return 1;
}

MemCB MemCardCallback(MemCB func) {
    MemCB previous;

    previous = callback;
    callback = func;
    return previous;
}

long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir, long *files, long ofs, long max) {
    MemcardEntry entries[MEMCARD_DATA_BLOCKS];
    long result;
    long count;
    int listed;
    int i;

    count = 0;
    result = usable(chan);
    if (result == McErrNone) {
        listed = memcard_list(name, entries, MEMCARD_DATA_BLOCKS);
        for (i = (int)ofs; i < listed && count < max; i++) {
            memset(&dir[count], 0, sizeof(dir[count]));
            memcpy(dir[count].name, entries[i].name, sizeof(dir[count].name));
            dir[count].attr = 0x51;
            dir[count].size = entries[i].size;
            dir[count].head = entries[i].first_block;
            count++;
        }
    }
    if (files != NULL) {
        *files = count;
    }
    trace("MemCardGetDirentry", chan, result);
    return result;
}
