#include <string.h>
#include "format.h"
#include "options.h"

static const char *const units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
#define NUNITS  (sizeof units / sizeof units[0])

dusize_t du_alloc(unsigned long size, unsigned long cluster)
{
    unsigned long n;

    if (cluster == 0)
        return size;
    /* size + cluster - 1 could wrap 32 bits; this cannot */
    n = size / cluster + (size % cluster != 0);
    return (dusize_t)n * cluster;
}

void fmt_u64(char *buf, dusize_t v)
{
    char tmp[21];
    int n = 0;

    do {
        tmp[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v != 0);
    while (n > 0)
        *buf++ = tmp[--n];
    *buf = '\0';
}

void fmt_kib(char *buf, dusize_t bytes)
{
    /* bytes + 1023 would wrap for the largest 64-bit values */
    fmt_u64(buf, bytes / 1024 + (bytes % 1024 != 0));
}

void fmt_human(char *buf, dusize_t bytes)
{
    dusize_t div = 1024, tenths;
    unsigned u;

    if (bytes < 1024) {
        fmt_u64(buf, bytes);
        strcat(buf, units[0]);
        return;
    }
    /* tenths of the unit, rounded up; move up a unit once it would
     * show as 1024.0.  Splitting off the remainder keeps bytes * 10
     * from overflowing for any 64-bit value. */
    for (u = 1; ; u++, div *= 1024) {
        tenths = bytes / div * 10 + (bytes % div * 10 + div - 1) / div;
        if (tenths < 10240 || u == NUNITS - 1)
            break;
    }
    fmt_u64(buf, tenths / 10);
    buf += strlen(buf);
    *buf++ = '.';
    *buf++ = (char)('0' + (int)(tenths % 10));
    strcpy(buf, units[u]);
}

void fmt_size(char *buf, dusize_t bytes, int unit)
{
    char tmp[24];
    int n, pad;

    if (unit == UNIT_HUMAN)
        fmt_human(tmp, bytes);
    else if (unit == UNIT_BYTES)
        fmt_u64(tmp, bytes);
    else
        fmt_kib(tmp, bytes);
    n = (int)strlen(tmp);
    for (pad = FMT_SIZEWIDTH - n; pad > 0; pad--)
        *buf++ = ' ';
    memcpy(buf, tmp, (size_t)n + 1);
}
