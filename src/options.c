#include <string.h>
#include "options.h"
#include "out.h"

static int is_letter(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* ASCII case-insensitive equality */
static int same(const char *a, const char *b)
{
    for (; *a && lower(*a) == lower(*b); a++, b++)
        ;
    return lower(*a) == lower(*b);
}

int operand_drive(const char *arg)
{
    const char *colon = strchr(arg, ':');

    if (arg[0] == '\0')
        return DRIVE_BAD;
    if (colon == NULL)
        return DRIVE_CURRENT;
    if (colon != arg + 1 || !is_letter(arg[0]) || strchr(colon + 1, ':'))
        return DRIVE_BAD;
    return lower(arg[0]) - 'a';
}

const char *check_operand(const char *arg)
{
    if (operand_drive(arg) == DRIVE_BAD)
        return "invalid drive or path";
    if (strchr(arg, '*') || strchr(arg, '?'))
        return "wildcards are not supported";
    return NULL;
}

int parse_options(int argc, char **argv, struct options *o, FILE *err)
{
    int i, endopts = 0;
    const char *a, *why;
    char opt[2];

    o->all = 0;
    o->summary = 0;
    o->total = 0;
    o->apparent = 0;
    o->unit = UNIT_KIB;

    for (i = 1; i < argc; i++) {
        a = argv[i];

        if (endopts || a[0] != '-') {
            why = check_operand(a);
            if (why != NULL) {
                put(err, "du: ", why, " '", a, "'\n", NULL);
                return OPT_USAGE;
            }
            continue;
        }
        if (a[1] == '\0') {
            fputs("du: invalid option '-'\n", err);
            return OPT_USAGE;
        }
        if (a[1] == '-') {
            if (a[2] == '\0')
                endopts = 1;
            else if (same(a, "--help"))
                return OPT_HELP;
            else if (same(a, "--version"))
                return OPT_VERSION;
            else if (same(a, "--apparent-size"))
                o->apparent = 1;
            else {
                put(err, "du: unrecognized option '", a, "'\n", NULL);
                return OPT_USAGE;
            }
            argv[i] = NULL;
            continue;
        }
        for (a++; *a; a++) {
            switch (lower(*a)) {
            case 'a':
                o->all = 1;
                break;
            case 's':
                o->summary = 1;
                break;
            case 'c':
                o->total = 1;
                break;
            case 'b':
                o->apparent = 1;
                o->unit = UNIT_BYTES;
                break;
            case 'h':
                o->unit = UNIT_HUMAN;
                break;
            case 'k':
                o->unit = UNIT_KIB;
                break;
            case '?':
                return OPT_HELP;
            default:
                opt[0] = *a;
                opt[1] = '\0';
                put(err, "du: invalid option -- '", opt, "'\n", NULL);
                return OPT_USAGE;
            }
        }
        argv[i] = NULL;
    }
    if (o->all && o->summary) {
        fputs("du: -a and -s cannot be used together\n", err);
        return OPT_USAGE;
    }
    return OPT_RUN;
}
