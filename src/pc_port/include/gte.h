#ifndef PC_PORT_GTE_H
#define PC_PORT_GTE_H

/* Host GTE. Replaces src/include/gte.h on the host include path: the command macros
 * come from the host inline_c.h as C calls. Nothing here emits a MIPS opcode. */

#include <inline_c.h>
#include <libgte.h>

void gte_set_rotation_identity(void);
void gte_set_translation(int tx, int ty, int tz);

/* Writes signed 16-bit screen coordinates. Returns 0 when z is in front,
 * and -1 when the vertex fails the front-of-camera test. */
int gte_project(int vx, int vy, int vz, short *sx, short *sy);

/* Identity at (160, 120), a vertex to the right, and a vertex behind. */
int gte_startup_check(void);

#endif
