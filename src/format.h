#ifndef FORMAT_H
#define FORMAT_H

/*
 * Space arithmetic and output formatting.  Portable: no DOS calls.
 *
 * Totals are 64-bit: a tree can hold more than 4 GiB of files (several
 * operands, or a tree's logical lengths), and 64 bits cannot overflow
 * for anything DOS can store.  Open Watcom provides unsigned long long
 * for 16-bit targets with integer-only helper routines.
 */

typedef unsigned long long dusize_t;

/* The space a file of size bytes is allocated on a volume whose clusters
 * are cluster bytes: size rounded up to a whole number of clusters, so 0
 * for an empty file.  cluster 0 means unknown, and gives size. */
dusize_t du_alloc(unsigned long size, unsigned long cluster);

/* v in decimal.  buf needs 21 bytes. */
void fmt_u64(char *buf, dusize_t v);

/* bytes as 1 KiB blocks, rounded up. */
void fmt_kib(char *buf, dusize_t bytes);

/* bytes human-readable: "512B", "1.5KiB", "122.0MiB", ... rounded up. */
void fmt_human(char *buf, dusize_t bytes);

/* Width of the size column: sizes are right-aligned in it, so that DOS's
 * SORT orders lines by size, and the path follows after two spaces. */
#define FMT_SIZEWIDTH  9

/* bytes in unit (UNIT_* of options.h), right-aligned to FMT_SIZEWIDTH
 * (longer values are not cut).  buf needs 24 bytes. */
void fmt_size(char *buf, dusize_t bytes, int unit);

#endif
