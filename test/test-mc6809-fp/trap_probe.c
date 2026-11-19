/* Traps become calls of abort().
 *
 * The 6809 has no trap instruction to spare, so __builtin_trap,
 * __builtin_debugtrap and the overflow traps -ftrapv inserts (built with
 * -ftrapv here) are calls of abort(), which raises SIGABRT.  Catch it, and
 * check that each trap fires, that an overflowing signed add, subtract,
 * multiply and negation trap, and that the same operations without
 * overflow do not.  The handler leaves by siglongjmp, since raise()
 * blocks SIGABRT while its handler runs.
 *
 * Returns non-zero if any sub-test fails.
 */
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

static sigjmp_buf env;
static volatile long big = LONG_MAX, small = LONG_MIN, one = 1, two = 2;
static volatile long sink;

static void on_abort(int sig) {
    (void)sig;
    siglongjmp(env, 1);  /* restores the mask raise() set */
}

/* Run op; return 1 if it called abort(). */
static int traps(void (*op)(void)) {
    signal(SIGABRT, on_abort);
    if (sigsetjmp(env, 1))
        return 1;
    op();
    return 0;
}

static void do_trap(void) { __builtin_trap(); }
static void do_debugtrap(void) { __builtin_debugtrap(); }
static void add_overflow(void) { sink = big + one; }
static void sub_overflow(void) { sink = small - one; }
static void mul_overflow(void) { sink = big * two; }
static void neg_overflow(void) { sink = -small; }
static void add_ok(void) { sink = big - one + one; }
static void sub_ok(void) { sink = small + one - one; }
static void mul_ok(void) { sink = (big / two) * two; }
static void neg_ok(void) { sink = -big; }

int main(void) {
    static const struct {
        const char *name;
        void (*op)(void);
        int want;
    } cases[] = {
        { "__builtin_trap", do_trap, 1 },
        { "__builtin_debugtrap", do_debugtrap, 1 },
        { "add overflow", add_overflow, 1 },
        { "subtract overflow", sub_overflow, 1 },
        { "multiply overflow", mul_overflow, 1 },
        { "negate overflow", neg_overflow, 1 },
        { "add", add_ok, 0 },
        { "subtract", sub_ok, 0 },
        { "multiply", mul_ok, 0 },
        { "negate", neg_ok, 0 },
    };
    int e = 0;
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int got = traps(cases[i].op);
        if (got != cases[i].want) {
            printf("FAIL: %s %s\n", cases[i].name, got ? "trapped" : "did not trap");
            e++;
        }
    }
    printf("%d failures\n", e);
    return e != 0;
}
