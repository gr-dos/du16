#ifndef DU_H
#define DU_H

#include <stdio.h>
#include "format.h"
#include "options.h"

/* exit statuses, also used as "worst so far" */
#define RC_OK     0
#define RC_FAIL   1     /* an operand, or part of a tree, could not be read */
#define RC_FATAL  2     /* usage error, out of memory or output write error */

/* The DOS directory search record (INT 21h 4Eh/4Fh).  On DOS it is
 * libc16's struct find_t; the host tests use the same members. */
#ifdef __WATCOMC__
#include <dos.h>
typedef struct find_t du_find;
#else
typedef struct {
    char reserved[21];
    char attrib;
    unsigned wr_time;
    unsigned wr_date;
    long size;
    char name[13];
} du_find;
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_VOLID  0x08
#define _A_SUBDIR 0x10
#endif

/* Everything a directory search should return: plain, read-only and
 * archive entries always match, so this adds hidden, system and
 * directory entries. */
#define DU_ATTRS  (_A_HIDDEN | _A_SYSTEM | _A_SUBDIR)

/* DOS error codes du tells apart */
#define DOSERR_FILE_NOT_FOUND   2
#define DOSERR_PATH_NOT_FOUND   3
#define DOSERR_ACCESS_DENIED    5
#define DOSERR_INVALID_DRIVE   15
#define DOSERR_NOT_READY       21
#define DOSERR_NO_MORE_FILES   18
#define DOSERR_FAIL            83   /* critical error answered Fail */

/* Longest path du handles, with its terminator.  DOS's own limit is
 * lower (a 64-byte directory path), so this is only reached on a disk
 * DOS itself could not have made: a deeper directory is reported and
 * skipped.  It also bounds the depth of a walk (each level is at least
 * two characters), so a malformed disk whose directories loop cannot
 * make du run forever or overrun its stack. */
#define DU_PATHMAX   128
#define DU_MAXDEPTH  64     /* more than (DU_PATHMAX - 4) / 2 */

/*
 * The DOS boundary.  dosfs.c supplies the DOS calls; the host tests
 * supply a mock disk.  Drives are 0 for A:, 1 for B:, ...
 */
struct du_ops {
    int (*current_drive)(void);
    /* the drive's bytes per cluster, or 0 if invalid or not ready */
    unsigned long (*cluster_size)(int drive);
    /* the absolute path of path in buf, as libc16's _fullpath; NULL if
     * it does not fit in size bytes or the drive is invalid */
    char *(*fullpath)(char *buf, const char *path, unsigned size);
    /* 0, or a DOS error code */
    unsigned (*findfirst)(const char *path, unsigned attr, du_find *f);
    unsigned (*findnext)(du_find *f);
};

/*
 * Report the operands left non-NULL in argv[1..argc-1] by
 * parse_options, or the current directory if there are none.  Lines go
 * to out and diagnostics to err.  Returns the exit status.
 */
int du_run(const struct options *o, int argc, char **argv,
           const struct du_ops *ops, FILE *out, FILE *err);

/* Nonzero if the absolute path a is b or is inside it (case-insensitive,
 * so C:\DOS is inside c:\).  Exposed for the tests. */
int du_within(const char *a, const char *b);

#endif
