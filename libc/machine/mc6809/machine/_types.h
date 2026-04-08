/*
 * machine/_types.h — MC6809 type overrides for picolibc
 *
 * Picolibc's generic <sys/_types.h> defaults __off_t and __fpos_t to
 * __int64_t, which forces every file-position operation in stdio to
 * pull in 64-bit arithmetic. On the MC6809 (and the broader 6x09
 * ecosystem) `long long` is not a real first-class type yet — gcc6809
 * itself uses 4-byte long long, and LLVM-MC6809 doesn't yet have the
 * regalloc / register-bank plumbing to handle s64 vregs cleanly.
 *
 * Override the file-position types to be 32-bit (long, SImode). This
 * matches the rest of the 6809 ecosystem and unblocks the picolibc
 * fseek / ftell / fgetpos family without waiting for the broader
 * long-long roadmap to land. 32-bit file offsets give a 4 GB ceiling,
 * which is irrelevant on a target with no real filesystem.
 *
 * When/if the long-long roadmap lands and __int64_t becomes a fully
 * supported type, this override can be removed and __off_t will revert
 * to the standard 64-bit form.
 */

#ifndef _MACHINE__TYPES_H
#define _MACHINE__TYPES_H

#include <machine/_default_types.h>

#define __machine_off_t_defined
typedef __int32_t __off_t;

#define __machine_fpos_t_defined
typedef __int32_t __fpos_t;

#endif /* _MACHINE__TYPES_H */
