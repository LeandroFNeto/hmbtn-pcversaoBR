#ifndef EVENT_GUARD_H
#define EVENT_GUARD_H

/* HMBTN_DISABLE_EVENTS=1: skip the farm event dispatcher and clear the cinematic lock. */

void event_guard_apply(void);
int event_guard_self_check(void);

#endif
