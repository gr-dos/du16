/* Host-side tests for options.c. */

#include <stdarg.h>
#include <string.h>
#include "options.h"
#include "unit.h"

static char *args[16];
static char store[16][32];
static int nargs;
static struct options o;
static FILE *sink;

/* parse "du" followed by the given arguments, up to NULL */
static int parse(const char *a, ...)
{
    va_list ap;

    nargs = 0;
    strcpy(store[nargs], "du");
    args[nargs] = store[nargs];
    nargs++;
    va_start(ap, a);
    for (; a != NULL; a = va_arg(ap, const char *)) {
        strcpy(store[nargs], a);
        args[nargs] = store[nargs];
        nargs++;
    }
    va_end(ap);
    return parse_options(nargs, args, &o, sink);
}

/* the operands left, joined by spaces */
static const char *left(void)
{
    static char buf[256];
    int i;

    buf[0] = '\0';
    for (i = 1; i < nargs; i++) {
        if (args[i] == NULL)
            continue;
        if (buf[0])
            strcat(buf, " ");
        strcat(buf, args[i]);
    }
    return buf;
}

int main(void)
{
    sink = fopen("/dev/null", "w");

    /* defaults */
    CHECK(parse(NULL) == OPT_RUN);
    CHECK(!o.all && !o.summary && !o.total && !o.apparent);
    CHECK(o.unit == UNIT_KIB);

    /* each option, upper and lower case, grouped */
    CHECK(parse("-a", NULL) == OPT_RUN && o.all);
    CHECK(parse("-A", NULL) == OPT_RUN && o.all);
    CHECK(parse("-s", NULL) == OPT_RUN && o.summary);
    CHECK(parse("-S", NULL) == OPT_RUN && o.summary);
    CHECK(parse("-c", NULL) == OPT_RUN && o.total);
    CHECK(parse("-h", NULL) == OPT_RUN && o.unit == UNIT_HUMAN);
    CHECK(parse("-H", NULL) == OPT_RUN && o.unit == UNIT_HUMAN);
    CHECK(parse("-b", NULL) == OPT_RUN && o.apparent && o.unit == UNIT_BYTES);
    CHECK(parse("-sch", NULL) == OPT_RUN && o.summary && o.total &&
          o.unit == UNIT_HUMAN);
    CHECK(parse("-ac", "-k", NULL) == OPT_RUN && o.all && o.total &&
          o.unit == UNIT_KIB);

    /* units: the last one wins; -b also counts file lengths */
    CHECK(parse("-h", "-k", NULL) == OPT_RUN && o.unit == UNIT_KIB);
    CHECK(parse("-k", "-h", NULL) == OPT_RUN && o.unit == UNIT_HUMAN);
    CHECK(parse("-bh", NULL) == OPT_RUN && o.apparent && o.unit == UNIT_HUMAN);
    CHECK(parse("-bk", NULL) == OPT_RUN && o.apparent && o.unit == UNIT_KIB);
    CHECK(parse("-hb", NULL) == OPT_RUN && o.unit == UNIT_BYTES);
    CHECK(parse("--apparent-size", NULL) == OPT_RUN && o.apparent &&
          o.unit == UNIT_KIB);
    CHECK(parse("--APPARENT-SIZE", "-h", NULL) == OPT_RUN && o.apparent &&
          o.unit == UNIT_HUMAN);

    /* help and version; -h is never help */
    CHECK(parse("--help", NULL) == OPT_HELP);
    CHECK(parse("--HELP", NULL) == OPT_HELP);
    CHECK(parse("-?", NULL) == OPT_HELP);
    CHECK(parse("-a?", NULL) == OPT_HELP);
    CHECK(parse("--version", NULL) == OPT_VERSION);
    CHECK(parse("--Version", NULL) == OPT_VERSION);
    CHECK(parse("x:\\", "--help", NULL) == OPT_HELP);

    /* usage errors */
    CHECK(parse("-x", NULL) == OPT_USAGE);
    CHECK(parse("-ax", NULL) == OPT_USAGE);
    CHECK(parse("-", NULL) == OPT_USAGE);
    CHECK(parse("--bogus", NULL) == OPT_USAGE);
    CHECK(parse("--all", NULL) == OPT_USAGE);
    CHECK(parse("-as", NULL) == OPT_USAGE);
    CHECK(parse("-a", "-s", NULL) == OPT_USAGE);
    CHECK(parse("1:", NULL) == OPT_USAGE);
    CHECK(parse("c:\\dos", "foo:", NULL) == OPT_USAGE);
    CHECK(parse("c:\\a:b", NULL) == OPT_USAGE);
    CHECK(parse("*.*", NULL) == OPT_USAGE);
    CHECK(parse("c:\\dos\\*.exe", NULL) == OPT_USAGE);
    CHECK(parse("FILE?.TXT", NULL) == OPT_USAGE);
    /* the operand check comes before the run, wherever it is */
    CHECK(parse("c:", "*", "-a", NULL) == OPT_USAGE);

    /* operands are kept in order, options removed */
    CHECK(parse("-s", "C:\\", "-c", "a:", ".", NULL) == OPT_RUN);
    CHECK(strcmp(left(), "C:\\ a: .") == 0);
    CHECK(parse("--", "-a", NULL) == OPT_RUN && !o.all);
    CHECK(strcmp(left(), "-a") == 0);
    CHECK(parse("-a", "--", "--", NULL) == OPT_RUN && o.all);
    CHECK(strcmp(left(), "--") == 0);

    /* drives */
    CHECK(operand_drive("C:") == 2);
    CHECK(operand_drive("c:") == 2);
    CHECK(operand_drive("a:\\") == 0);
    CHECK(operand_drive("Z:FOO") == 25);
    CHECK(operand_drive(".") == DRIVE_CURRENT);
    CHECK(operand_drive("\\DOS") == DRIVE_CURRENT);
    CHECK(operand_drive("..\\X") == DRIVE_CURRENT);
    CHECK(operand_drive("") == DRIVE_BAD);
    CHECK(operand_drive("1:") == DRIVE_BAD);
    CHECK(operand_drive("CC:") == DRIVE_BAD);
    CHECK(operand_drive(":") == DRIVE_BAD);
    CHECK(check_operand("C:\\DOS") == NULL);
    CHECK(check_operand("C:\\DOS\\*") != NULL);
    CHECK(check_operand("") != NULL);

    fclose(sink);
    return UNIT_DONE();
}
