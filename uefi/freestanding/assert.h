/* assert.h for a freestanding EFI build. See README in this directory. */

#ifndef _FREESTANDING_ASSERT_H
#define _FREESTANDING_ASSERT_H

#include <uefi.h>

/* There is nowhere useful to report to this early, and the firmware may not
 * have a console yet, so a failed assertion stops the machine. */
#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))

__attribute__((noreturn)) void __assert_fail(const char *expr, const char *file, int line);

#endif
