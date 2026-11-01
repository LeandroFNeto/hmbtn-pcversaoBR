/* The card file's atomic replace, apart from memcard.c because windows.h clashes with
 * the PSY-Q types. */

#define PC_PORT_NO_PSYQ_TYPES

#include <memcard.h>

#ifdef _WIN32
#include <windows.h>

unsigned long memcard_replace_file(const char *tmp, const char *path) {
    if (MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return 0;
    }
    return (unsigned long)GetLastError();
}
#else
#include <errno.h>
#include <stdio.h>

unsigned long memcard_replace_file(const char *tmp, const char *path) {
    return rename(tmp, path) == 0 ? 0 : (unsigned long)errno;
}
#endif
