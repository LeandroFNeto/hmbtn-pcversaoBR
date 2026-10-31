/* libmcrd: no memory card is inserted. */

#include <libmcrd.h>

void MemCardInit(long val) {
    (void)val;
}

void MemCardStart(void) {
}

long MemCardExist(long chan) {
    (void)chan;
    return 0;
}

long MemCardAccept(long chan) {
    (void)chan;
    return 0;
}

long MemCardReadFile(long chan, char *file, u_long *adrs, long ofs, long bytes) {
    (void)chan;
    (void)file;
    (void)adrs;
    (void)ofs;
    (void)bytes;
    return McErrCardNotExist;
}

long MemCardCreateFile(long chan, char *file, long blocks) {
    (void)chan;
    (void)file;
    (void)blocks;
    return McErrCardNotExist;
}

long MemCardWriteFile(long chan, char *file, u_long *adrs, long ofs, long bytes) {
    (void)chan;
    (void)file;
    (void)adrs;
    (void)ofs;
    (void)bytes;
    return McErrCardNotExist;
}

long MemCardFormat(long chan) {
    (void)chan;
    return McErrCardNotExist;
}

long MemCardUnformat(long chan) {
    (void)chan;
    return McErrCardNotExist;
}

long MemCardSync(long mode, long *cmds, long *rslt) {
    (void)mode;
    if (cmds != 0) {
        *cmds = 0;
    }
    if (rslt != 0) {
        *rslt = McErrCardNotExist;
    }
    return 1;
}

long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir, long *files, long ofs, long max) {
    (void)chan;
    (void)name;
    (void)dir;
    (void)ofs;
    (void)max;
    if (files != 0) {
        *files = 0;
    }
    return McErrCardNotExist;
}
