/*
 * DOS test of du's critical-error handling (src/dosfs.c), run by
 * tests/run.sh.  DOSBox-X never raises a critical error for its drives,
 * so this checks the parts that can be checked there: the handler
 * answers Fail and preserves the other registers, and each DOS call
 * leaves INT 24h as it found it, whether it succeeds or not.  Prints one
 * line per failed check and "ok N" at the end; ERRORLEVEL is the number
 * of failures.
 */

#include <dos.h>
#include <stdio.h>
#include "dosfs.h"

/* INT 24h with AX and BX given; returns AX and stores BX in *bx. */
static unsigned raise24(unsigned ax, unsigned bx, unsigned *bxout);
#pragma aux raise24 =    \
    "push si"            \
    "mov si,cx"          \
    "int 24h"            \
    "mov [si],bx"        \
    "pop si"             \
    __parm [__ax] [__bx] [__cx] __value [__ax] __modify [__bx];

typedef void (__interrupt __far *isr)();

static int checks, failures;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        fputs("FAIL: ", stdout);
        fputs(what, stdout);
        fputs("\n", stdout);
    }
}

int main(void)
{
    isr before;
    du_find f;
    unsigned bx;
    char n[3];

    before = _dos_getvect(0x24);
    check(dos_cluster_size(2) != 0, "C: has a cluster size");
    check(_dos_getvect(0x24) == before, "INT 24h restored after C:");
    check(dos_cluster_size(24) == 0, "Y: is invalid");
    check(_dos_getvect(0x24) == before, "INT 24h restored after Y:");
    check(dos_findfirst("C:\\*.*", 0, &f) == 0, "C:\\*.* finds a file");
    dos_findnext(&f);
    check(_dos_getvect(0x24) == before, "INT 24h restored after search");
    check(dos_findfirst("C:\\NOSUCH\\*.*", 0, &f) == 3, "no C:\\NOSUCH");
    check(_dos_getvect(0x24) == before, "INT 24h restored after error");

    _dos_setvect(0x24, dos_fail_handler);
    check(raise24(0x1a00, 0x1234, &bx) == 0x1a03, "handler returns AL=3");
    check(raise24(0x8001, 0x5678, &bx) == 0x8003, "handler keeps AH");
    check(bx == 0x5678, "handler keeps BX");
    _dos_setvect(0x24, before);
    check(_dos_getvect(0x24) == before, "INT 24h set back by the test");

    n[0] = (char)('0' + checks / 10);
    n[1] = (char)('0' + checks % 10);
    n[2] = '\0';
    if (failures == 0) {
        fputs("ok ", stdout);
        fputs(n, stdout);
        fputs("\n", stdout);
    }
    return failures;
}
