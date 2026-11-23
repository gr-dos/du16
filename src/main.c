/*
 * du - report disk usage of DOS files and directories
 *
 * A Unix du-style report for 16-bit MS-DOS (Open Watcom C with
 * libc16): the space files take on the disk, rounded up to the
 * volume's clusters, totalled by directory.  It only reads the disk.
 */

#include <stdio.h>
#include "dosfs.h"
#include "du.h"
#include "options.h"

#ifndef VERSION
#define VERSION   "unknown"
#endif

static const struct du_ops dos_ops = {
    dos_current_drive,
    dos_cluster_size,
    dos_fullpath,
    dos_findfirst,
    dos_findnext
};

static void usage(void)
{
    fputs(
        "Usage: du [OPTION]... [FILE-OR-DIRECTORY]...\n"
        "Report the disk space used by files and directories.\n"
        "\n"
        "  -a               also a line for each file\n"
        "  -s               only a total for each operand\n"
        "  -c               a grand total at the end\n"
        "  -k               sizes in 1 KiB blocks (default)\n"
        "  -h               human-readable sizes (B, KiB, MiB, GiB)\n"
        "  -b               apparent sizes, in bytes\n"
        "  --apparent-size  count file lengths, not allocated space\n"
        "  -?, --help       display help and exit\n"
        "  --version        display program version and exit\n"
        "  --               end option processing\n"
        "\n"
        "Sizes are the space allocated to files, rounded up to the\n"
        "drive's cluster size; directories' own clusters are not counted.\n"
        "With no operand, report the current directory.\n"
        "\n"
        "ERRORLEVEL is 0 on success, 1 if anything could not be read, and\n"
        "2 for a usage or write error.\n",
        stdout);
}

static int finish(void)
{
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fputs("du: write error\n", stderr);
        return RC_FATAL;
    }
    return RC_OK;
}

int main(int argc, char **argv)
{
    struct options o;

    switch (parse_options(argc, argv, &o, stderr)) {
    case OPT_HELP:
        usage();
        return finish();
    case OPT_VERSION:
        fputs("du (dos-du16) " VERSION "\n", stdout);
        return finish();
    case OPT_USAGE:
        fputs("Try 'du --help' for more information.\n", stderr);
        return RC_FATAL;
    }
    return du_run(&o, argc, argv, &dos_ops, stdout, stderr);
}
