/* libapi's kernel card calls. On the PS1 libmcrd sits on top of them; the host's libmcrd
 * works on the card image directly, and the game never calls these. */

#include <libapi.h>

void InitCARD(long val) {
    (void)val;
}

long StartCARD(void) {
    return 1;
}

long StopCARD(void) {
    return 1;
}

void _bu_init(void) {
}

long _card_info(long chan) {
    (void)chan;
    return 1;
}

long _card_load(long chan) {
    (void)chan;
    return 1;
}

long _card_clear(long chan) {
    (void)chan;
    return 1;
}
