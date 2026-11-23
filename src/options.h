#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdio.h>

/* Command-line parsing.  Portable: no DOS calls. */

/* Units for sizes; of -b, -k and -h the last one wins. */
#define UNIT_KIB    0       /* -k: 1 KiB blocks, rounded up (default) */
#define UNIT_HUMAN  1       /* -h: B, KiB, MiB, GiB, TiB */
#define UNIT_BYTES  2       /* -b: bytes */

struct options {
    int all;            /* -a: a line for every file too */
    int summary;        /* -s: only a line for each operand */
    int total;          /* -c: a grand total line at the end */
    int apparent;       /* -b, --apparent-size: file lengths, not allocation */
    int unit;           /* UNIT_* */
};

/* parse_options results */
#define OPT_RUN      0
#define OPT_HELP     1
#define OPT_VERSION  2
#define OPT_USAGE    3      /* a diagnostic has been written to err */

/*
 * Parse argv into o.  Options are case-insensitive and short ones may be
 * grouped (-sc).  Options (and "--") are removed by setting their argv
 * entries to NULL, leaving the operands in order.  Operands are checked
 * with check_operand, so a malformed one is a usage error before any
 * disk is touched.
 */
int parse_options(int argc, char **argv, struct options *o, FILE *err);

/* operand_drive results other than a drive index */
#define DRIVE_CURRENT  (-1)     /* no drive prefix: the current drive */
#define DRIVE_BAD      (-2)     /* malformed */

/* The drive an operand names: 0 for A: ... 25 for Z:, or one of the
 * above.  "C:", "c:\FOO" and "C:FOO" are drive C; ".", "FOO" and
 * "\FOO" are the current drive; "1:", "" and "FOO:" are malformed. */
int operand_drive(const char *arg);

/* NULL if arg is a usable operand, else why not ("invalid drive or
 * path", "wildcards are not supported"). */
const char *check_operand(const char *arg);

#endif
