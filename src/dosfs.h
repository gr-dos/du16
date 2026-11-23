#ifndef DOSFS_H
#define DOSFS_H

#include "du.h"

/* The DOS calls behind struct du_ops (INT 21h).  Drives are 0 for A:. */

/* AH=19h: the current drive. */
int dos_current_drive(void);

/* AH=36h: sectors per cluster times bytes per sector, or 0 if DOS
 * reports the drive invalid or not ready (AX=FFFFh). */
unsigned long dos_cluster_size(int drive);

/* libc16's _fullpath. */
char *dos_fullpath(char *buf, const char *path, unsigned size);

/* AH=4Eh and AH=4Fh, through libc16: 0 or the DOS error code. */
unsigned dos_findfirst(const char *path, unsigned attr, du_find *f);
unsigned dos_findnext(du_find *f);

/*
 * On DOS 3.0 and later, each of the calls above that reads a disk does
 * so with a critical-error (INT 24h) handler that answers Fail, so an
 * empty floppy drive or a bad sector gives an error instead of an
 * "Abort, Retry, Fail?" prompt that would stop an unattended run.  The
 * previous handler is put back as soon as the call returns.
 */
void __interrupt __far dos_fail_handler();

#endif
