#include <dos.h>
#include <stdlib.h>
#include "dosfs.h"

/*
 * Critical errors (INT 24h).  Reading a removable or otherwise unready
 * drive, or a bad sector, can make DOS raise a critical error, whose
 * default handler asks "Abort, Retry, Fail?" and waits for a key.  For
 * the length of each call that reads a disk, du installs a handler that
 * answers Fail, so the call returns an error (83, or AX=FFFFh from
 * AH=36h) and du reports it and goes on.  The previous handler is put
 * back as soon as the call returns, so critical errors anywhere else,
 * such as writing the report to a floppy, still prompt as usual.  If du
 * is ended during a call, DOS restores INT 24h from the PSP itself.
 *
 * Fail (AL=3) is DOS 3.0 and later; on DOS 2.x the handler is not
 * installed and DOS's own handling applies.
 */

#define CRITICAL_ERROR 0x24
#define FAIL           3

typedef void (__interrupt __far *isr)();

/* The registers an Open Watcom __interrupt function saves, as its
 * argument sees them; the values in it are restored on return. */
struct isr_regs {
    unsigned gs, fs, es, ds, di, si, bp, sp, bx, dx, cx, ax, ip, cs, flags;
};

/* Answer Fail.  Calls nothing: DOS is not reentrant here. */
void __interrupt __far dos_fail_handler(struct isr_regs r)
{
    r.ax = (r.ax & 0xff00) | FAIL;
}

static isr saved;

static int guard_on(void)
{
    static int major = -1;
    union REGS r;

    if (major < 0) {
        r.x.ax = 0x3000;
        int86(0x21, &r, &r);
        major = r.h.al;
    }
    if (major < 3)
        return 0;
    saved = _dos_getvect(CRITICAL_ERROR);
    _dos_setvect(CRITICAL_ERROR, dos_fail_handler);
    return 1;
}

static void guard_off(int on)
{
    if (on)
        _dos_setvect(CRITICAL_ERROR, saved);
}

int dos_current_drive(void)
{
    union REGS r;

    r.h.ah = 0x19;
    int86(0x21, &r, &r);
    return r.h.al;
}

unsigned long dos_cluster_size(int drive)
{
    union REGS r;
    int on = guard_on();

    r.h.ah = 0x36;
    r.h.dl = (unsigned char)(drive + 1);
    int86(0x21, &r, &r);
    guard_off(on);
    if (r.x.ax == 0xffff)
        return 0;
    return (unsigned long)r.x.ax * r.x.cx;
}

char *dos_fullpath(char *buf, const char *path, unsigned size)
{
    return _fullpath(buf, path, size);
}

unsigned dos_findfirst(const char *path, unsigned attr, du_find *f)
{
    int on = guard_on();
    unsigned e = _dos_findfirst(path, attr, f);

    guard_off(on);
    return e;
}

unsigned dos_findnext(du_find *f)
{
    int on = guard_on();
    unsigned e = _dos_findnext(f);

    guard_off(on);
    return e;
}
