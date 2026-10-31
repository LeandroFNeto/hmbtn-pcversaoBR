#ifndef PC_PORT_SYS_TYPES_H
#define PC_PORT_SYS_TYPES_H

#include_next <sys/types.h>
#include <stddef.h>
#include <stdint.h>

/* Host files that include windows.h define PC_PORT_NO_PSYQ_TYPES: winsock declares
 * u_long as unsigned long, which is a different type from uint32_t. */
#ifndef PC_PORT_NO_PSYQ_TYPES
typedef uint8_t u_char;
typedef uint16_t u_short;
typedef uint32_t u_long;
typedef uint32_t u_int;
#endif

#endif
