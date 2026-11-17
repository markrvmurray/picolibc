/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The start-up code's vector table at $FFF0. A program installs an
 * interrupt handler by defining a function with the name the table
 * expects; a vector it leaves alone goes to the default trap.
 *
 * Two software interrupts are taken for real, since they need nothing
 * from the machine but the processor. The other vectors are checked by
 * reading the table: nothing here can raise an IRQ, FIRQ or NMI on every
 * simulator the tests run on.
 */

#include <stdio.h>

extern void _start(void);
extern void __swi3_trap(void);
extern void __unhandled_interrupt(void);

static volatile unsigned char swis, swi2s;
static volatile unsigned      mixed = 1;

/* Something for a handler to call, so that it has registers to save. */
static __attribute__((noinline)) unsigned
churn(unsigned x)
{
    return x * 7u + 3u;
}

__attribute__((interrupt("swi"))) void
__swi_handler(void)
{
    swis++;
    mixed = churn(mixed);
}

__attribute__((interrupt("swi2"))) void
__swi2_handler(void)
{
    swi2s++;
}

__attribute__((interrupt)) void
__irq_handler(void)
{
    mixed = churn(mixed);
}

__attribute__((interrupt("firq"))) void
__firq_handler(void)
{
    mixed = churn(mixed);
}

typedef void (*vector_t)(void);

static int
check(const char *name, unsigned addr, vector_t want)
{
    vector_t got = *(volatile vector_t *)addr;

    if (got == want)
        return 0;
    printf("%s vector at %#x is %p, not %p\n", name, addr, (void *)got, (void *)want);
    return 1;
}

int
main(void)
{
    int      bad = 0;
    unsigned before;

    bad += check("trap", 0xfff0, __unhandled_interrupt);
    bad += check("SWI3", 0xfff2, __swi3_trap);
    bad += check("SWI2", 0xfff4, __swi2_handler);
    bad += check("FIRQ", 0xfff6, __firq_handler);
    bad += check("IRQ", 0xfff8, __irq_handler);
    bad += check("SWI", 0xfffa, __swi_handler);
    bad += check("NMI", 0xfffc, __unhandled_interrupt);
    bad += check("reset", 0xfffe, _start);

    before = mixed;
    __asm__ volatile("swi");
    __asm__ volatile("swi2");
    __asm__ volatile("swi");

    if (swis != 2 || swi2s != 1) {
        printf("took %u SWI and %u SWI2, not 2 and 1\n", swis, swi2s);
        bad++;
    }
    if (mixed != churn(churn(before))) {
        printf("the SWI handler left %u, not %u\n", mixed, churn(churn(before)));
        bad++;
    }

    if (bad)
        return 1;
    printf("vectors ok\n");
    return 0;
}
