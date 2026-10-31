/* libapi: BIOS services. */

#include <libapi.h>

void SetMem(int n) {
    (void)n;
}

void SystemError(char c, long n) {
    (void)c;
    (void)n;
}

void ChangeClearPAD(long val) {
    (void)val;
}
