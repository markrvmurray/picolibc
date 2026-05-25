/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Mark Murray
 *
 * Bug #349 — long long (i64) string conversion and formatted IO.
 *
 * gcc6809 has no 64-bit integer type at all, so there is no reference
 * behaviour to match here: llvm-mc6809 is the first toolchain to offer
 * i64 on the 6809, and this test pins the standard we set.
 *
 * strtoull / strtoll are compiled into libc.a unconditionally, so they
 * are verified on every build.  The printf/scanf "ll" length modifier
 * is available exactly when stdio.h's own predicate holds — either the
 * io-long-long meson option is set (macro __IO_LONG_LONG) or the target
 * has long long no wider than long (a 64-bit host).  The formatted-IO
 * checks run only then, and the test skips them (still passing) when ll
 * IO is absent, so the integer-only mc6809 default build stays green.
 *
 * The failure-reporting path deliberately avoids %llu — it splits a
 * 64-bit value into two %08lx halves — so a failure is reported
 * correctly even in a build where ll IO is unavailable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Matches the gate in <stdio.h> (line ~583) that decides whether the
 * "ll" length modifier is honoured by printf/scanf. */
#if defined(__IO_LONG_LONG) || __SIZEOF_LONG_LONG__ == __SIZEOF_LONG__
#define HAVE_LL_IO 1
#endif

static int errors;

static void
fail_u64(const char *what, unsigned long long got, unsigned long long want)
{
    printf("FAIL %s: got %08lx%08lx want %08lx%08lx\n", what,
           (unsigned long)(got >> 32), (unsigned long)(got & 0xffffffffUL),
           (unsigned long)(want >> 32), (unsigned long)(want & 0xffffffffUL));
    errors++;
}

static void
check_u64(const char *what, unsigned long long got, unsigned long long want)
{
    if (got != want)
        fail_u64(what, got, want);
}

#ifdef HAVE_LL_IO
static void
check_str(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: got \"%s\" want \"%s\"\n", what, got, want);
        errors++;
    }
}
#endif

int
main(void)
{
    /* Always built, independent of io-long-long. */
    check_u64("strtoull max",
              strtoull("18446744073709551615", NULL, 10),
              18446744073709551615ULL);
    check_u64("strtoll min",
              (unsigned long long)strtoll("-9223372036854775808", NULL, 10),
              (unsigned long long)(-9223372036854775807LL - 1));
    check_u64("strtoull hex",
              strtoull("ffffffffffffffff", NULL, 16),
              0xffffffffffffffffULL);
    check_u64("strtoull octal",
              strtoull("01777777777777777777777", NULL, 8),
              0xffffffffffffffffULL);

#ifdef HAVE_LL_IO
    char buf[40];

    snprintf(buf, sizeof buf, "%llu", 18446744073709551615ULL);
    check_str("%llu max", buf, "18446744073709551615");

    snprintf(buf, sizeof buf, "%lld", -9223372036854775807LL - 1);
    check_str("%lld min", buf, "-9223372036854775808");

    snprintf(buf, sizeof buf, "%llx", 0x1122334455667788ULL);
    check_str("%llx", buf, "1122334455667788");

    /* A trailing %d must survive the wider %llu vararg slot. */
    snprintf(buf, sizeof buf, "%llu %d", 4294967296ULL, 42);
    check_str("vararg walk", buf, "4294967296 42");

    unsigned long long v = 0;
    if (sscanf("12345678901234567890", "%llu", &v) != 1)
        check_u64("sscanf count", 0, 1);
    check_u64("sscanf %llu", v, 12345678901234567890ULL);

    printf("ll IO ENABLED: %%llu/%%lld/%%llx + scanf verified\n");
#else
    printf("ll IO DISABLED: not built; strtoull/strtoll verified\n");
#endif

    if (errors) {
        printf("LLONG TEST FAILED (%d error%s)\n", errors,
               errors == 1 ? "" : "s");
        return 1;
    }
    printf("LLONG TEST PASSED\n");
    return 0;
}
