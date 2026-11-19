/* Floating point comparisons, NaN included.
 *
 * a > b becomes __gtXf2(a, b) > 0 and a >= b becomes __geXf2(a, b) >= 0,
 * so those two must return a negative value for an unordered pair while
 * the others return a positive one; with +1 from all of them, x > NaN and
 * x >= NaN were true.  Check the six comparison operators and the four
 * isgreater-family macros on every pair from a set of values with known
 * order -- -inf, -1, -0, +0, 1, +inf -- and NaN, in single and double
 * precision.
 *
 * Returns non-zero if any sub-test fails.
 */
#include <math.h>
#include <stdio.h>

#define N 7
/* rank in the order; -1 is NaN, unordered with everything */
static const int rank[N] = { 0, 1, 2, 2, 3, 4, -1 };

static volatile double dv[N];
static volatile float fv[N];

static int check(const char *what, int i, int j, int got, int want) {
    if (got == want)
        return 0;
    printf("FAIL: %s pair %d,%d: got %d want %d\n", what, i, j, got, want);
    return 1;
}

int main(void) {
    int e = 0;
#if defined(__FAST_MATH__) || __FINITE_MATH_ONLY__
    printf("skipping: NaN comparisons not meaningful under -ffinite-math-only\n");
    return 77;
#endif
    double one = 1.0, zero = 0.0;
    dv[0] = -one / zero; dv[1] = -1.0; dv[2] = -0.0; dv[3] = 0.0;
    dv[4] = 1.0; dv[5] = one / zero; dv[6] = zero / zero;
    for (int i = 0; i < N; i++)
        fv[i] = (float)dv[i];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int un = rank[i] < 0 || rank[j] < 0;
            int lt = !un && rank[i] < rank[j], gt = !un && rank[i] > rank[j];
            int eq = !un && rank[i] == rank[j];
            double a = dv[i], b = dv[j];
            float x = fv[i], y = fv[j];
            e += check("double <", i, j, a < b, lt);
            e += check("double <=", i, j, a <= b, lt || eq);
            e += check("double >", i, j, a > b, gt);
            e += check("double >=", i, j, a >= b, gt || eq);
            e += check("double ==", i, j, a == b, eq);
            e += check("double !=", i, j, a != b, !eq);
            e += check("double isgreater", i, j, isgreater(a, b) != 0, gt);
            e += check("double isgreaterequal", i, j, isgreaterequal(a, b) != 0, gt || eq);
            e += check("double isless", i, j, isless(a, b) != 0, lt);
            e += check("double islessequal", i, j, islessequal(a, b) != 0, lt || eq);
            e += check("float <", i, j, x < y, lt);
            e += check("float <=", i, j, x <= y, lt || eq);
            e += check("float >", i, j, x > y, gt);
            e += check("float >=", i, j, x >= y, gt || eq);
            e += check("float ==", i, j, x == y, eq);
            e += check("float !=", i, j, x != y, !eq);
            e += check("float isgreater", i, j, isgreater(x, y) != 0, gt);
            e += check("float isgreaterequal", i, j, isgreaterequal(x, y) != 0, gt || eq);
            e += check("float isless", i, j, isless(x, y) != 0, lt);
            e += check("float islessequal", i, j, islessequal(x, y) != 0, lt || eq);
        }
    }
    printf("%d failures\n", e);
    return e != 0;
}
