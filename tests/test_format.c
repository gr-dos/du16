/* Host-side tests for format.c: cluster rounding and number formats. */

#include <string.h>
#include "format.h"
#include "options.h"
#include "unit.h"

static char buf[64];

static const char *kib(dusize_t bytes)
{
    fmt_kib(buf, bytes);
    return buf;
}

static const char *human(dusize_t bytes)
{
    fmt_human(buf, bytes);
    return buf;
}

static const char *size(dusize_t bytes, int unit)
{
    fmt_size(buf, bytes, unit);
    return buf;
}

#define KIB(n)  ((dusize_t)(n) * 1024)
#define MIB(n)  (KIB(n) * 1024)
#define GIB(n)  (MIB(n) * 1024)
#define TIB(n)  (GIB(n) * 1024)

int main(void)
{
    /* cluster rounding: empty, below, at and above a boundary */
    CHECK(du_alloc(0, 512) == 0);
    CHECK(du_alloc(1, 512) == 512);
    CHECK(du_alloc(511, 512) == 512);
    CHECK(du_alloc(512, 512) == 512);
    CHECK(du_alloc(513, 512) == 1024);
    CHECK(du_alloc(1024, 1024) == 1024);
    CHECK(du_alloc(1025, 1024) == 2048);
    CHECK(du_alloc(2048, 2048) == 2048);
    CHECK(du_alloc(2049, 2048) == 4096);
    CHECK(du_alloc(32768, 32768) == 32768);
    CHECK(du_alloc(32769, 32768) == 65536);
    CHECK(du_alloc(1, 65536) == 65536);
    /* size + cluster - 1 would wrap 32 bits; the result needs 33 */
    CHECK(du_alloc(4294967295UL, 32768) == 4294967296ULL);
    CHECK(du_alloc(4294967295UL, 65536) == 4294967296ULL);
    CHECK(du_alloc(4294967295UL, 512) == 4294967296ULL);
    CHECK(du_alloc(4294967295UL, 1) == 4294967295ULL);
    CHECK(du_alloc(4294934528UL, 32768) == 4294934528ULL);
    /* unknown cluster size: the length itself */
    CHECK(du_alloc(1234, 0) == 1234);

    /* decimal */
    fmt_u64(buf, 0);
    CHECK(strcmp(buf, "0") == 0);
    fmt_u64(buf, 65535);
    CHECK(strcmp(buf, "65535") == 0);
    fmt_u64(buf, 4294967296ULL);
    CHECK(strcmp(buf, "4294967296") == 0);
    fmt_u64(buf, 18446744073709551615ULL);
    CHECK(strcmp(buf, "18446744073709551615") == 0);

    /* KiB blocks round up */
    CHECK(strcmp(kib(0), "0") == 0);
    CHECK(strcmp(kib(1), "1") == 0);
    CHECK(strcmp(kib(512), "1") == 0);
    CHECK(strcmp(kib(1024), "1") == 0);
    CHECK(strcmp(kib(1025), "2") == 0);
    CHECK(strcmp(kib(1536), "2") == 0);
    CHECK(strcmp(kib(368640), "360") == 0);
    CHECK(strcmp(kib(2147450880ULL), "2097120") == 0);
    CHECK(strcmp(kib(18446744073709551615ULL), "18014398509481984") == 0);

    /* human-readable: bytes below 1 KiB, then one decimal, rounded up */
    CHECK(strcmp(human(0), "0B") == 0);
    CHECK(strcmp(human(1), "1B") == 0);
    CHECK(strcmp(human(512), "512B") == 0);
    CHECK(strcmp(human(1023), "1023B") == 0);
    CHECK(strcmp(human(1024), "1.0KiB") == 0);
    CHECK(strcmp(human(1025), "1.1KiB") == 0);
    CHECK(strcmp(human(1536), "1.5KiB") == 0);
    CHECK(strcmp(human(KIB(1023) + 1), "1023.1KiB") == 0);
    CHECK(strcmp(human(MIB(1) - 1), "1.0MiB") == 0);
    CHECK(strcmp(human(MIB(1)), "1.0MiB") == 0);
    CHECK(strcmp(human(MIB(1) + 1), "1.1MiB") == 0);
    CHECK(strcmp(human(GIB(1) - 1), "1.0GiB") == 0);
    CHECK(strcmp(human(GIB(4)), "4.0GiB") == 0);
    CHECK(strcmp(human(TIB(1)), "1.0TiB") == 0);
    CHECK(strcmp(human(TIB(2048)), "2048.0TiB") == 0);
    CHECK(strcmp(human(18446744073709551615ULL), "16777216.0TiB") == 0);

    /* the size column: right-aligned to 9, longer values not cut */
    CHECK(strcmp(size(0, UNIT_KIB), "        0") == 0);
    CHECK(strcmp(size(1, UNIT_KIB), "        1") == 0);
    CHECK(strcmp(size(1025, UNIT_KIB), "        2") == 0);
    CHECK(strcmp(size(1025, UNIT_BYTES), "     1025") == 0);
    CHECK(strcmp(size(1536, UNIT_HUMAN), "   1.5KiB") == 0);
    CHECK(strcmp(size(KIB(1023) + 1, UNIT_HUMAN), "1023.1KiB") == 0);
    CHECK(strcmp(size(999999999, UNIT_BYTES), "999999999") == 0);
    CHECK(strcmp(size(4294967296ULL, UNIT_BYTES), "4294967296") == 0);
    CHECK(strcmp(size(18446744073709551615ULL, UNIT_BYTES),
                 "18446744073709551615") == 0);
    CHECK(strcmp(size(18446744073709551615ULL, UNIT_HUMAN),
                 "16777216.0TiB") == 0);

    return UNIT_DONE();
}
