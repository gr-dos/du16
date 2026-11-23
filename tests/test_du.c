/*
 * Host-side tests for du.c, against a mock disk: the DOS calls of
 * struct du_ops answered from a table of files, with DOS's rules for
 * attributes, "." and "..", and errors.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "du.h"
#include "unit.h"

/* ------------------------------------------------------------------ */
/* the mock disk */

struct entry {
    const char *path;       /* absolute, upper case */
    int attrib;
    unsigned long size;
};

/* error injections: a search of DIR\*.* fails at once, or after one
 * entry */
static const char *deny_dir;
static const char *broken_dir;

static const struct entry *disk;
static unsigned long clusters[26];
static int cluster_calls;
static char cwd[26][128];
static int current = 2;

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int same(const char *a, const char *b)
{
    for (; *a && lower(*a) == lower(*b); a++, b++)
        ;
    return lower(*a) == lower(*b);
}

static int mock_current(void)
{
    return current;
}

static unsigned long mock_cluster(int drive)
{
    cluster_calls++;
    return clusters[drive];
}

/* _fullpath: drive, "\", components with "." and ".." resolved, case
 * kept, no trailing "\" but on a root. */
static char *mock_fullpath(char *buf, const char *path, unsigned size)
{
    char tmp[512], *p, *q, *comp;
    int drive = current;
    size_t n;

    if (path[0] && path[1] == ':') {
        drive = lower(path[0]) - 'a';
        path += 2;
    }
    if (clusters[drive] == 0)
        return NULL;
    tmp[0] = '\0';
    if (*path != '\\' && *path != '/')
        strcpy(tmp, cwd[drive]);
    strcat(tmp, "\\");
    strcat(tmp, path);
    buf[0] = (char)('A' + drive);
    buf[1] = ':';
    buf[2] = '\0';
    p = tmp;
    n = 2;
    for (;;) {
        while (*p == '\\' || *p == '/')
            p++;
        if (*p == '\0')
            break;
        comp = p;
        while (*p && *p != '\\' && *p != '/')
            p++;
        if (*p)
            *p++ = '\0';
        if (strcmp(comp, ".") == 0)
            continue;
        if (strcmp(comp, "..") == 0) {
            q = strrchr(buf, '\\');
            if (q != NULL) {
                *q = '\0';
                n = (size_t)(q - buf);
            }
            continue;
        }
        if (n + 1 + strlen(comp) + 1 > size)
            return NULL;
        strcat(buf, "\\");
        strcat(buf, comp);
        n = strlen(buf);
    }
    if (n == 2) {
        if (size < 4)
            return NULL;
        strcat(buf, "\\");
    }
    return buf;
}

/* A search: the directory, and the next entry of the table to look at
 * (or -2 and -1 for "." and ".."), kept in the record's reserved bytes
 * as an index into this table. */
struct search {
    char dir[256];
    char name[64];          /* an exact name, or "" for *.* */
    unsigned attr;
    int next;
    int returned;
};
static struct search searches[256];
static int nsearches;

static void parent(char *dst, const char *path)
{
    const char *s = strrchr(path, '\\');

    memcpy(dst, path, (size_t)(s - path));
    dst[s - path] = '\0';
    if (strlen(dst) == 2)
        strcat(dst, "\\");
}

static void fill(du_find *f, const char *name, int attrib, unsigned long size)
{
    memset(f->name, 0, sizeof f->name);
    strcpy(f->name, name);
    f->attrib = (char)attrib;
    f->size = (long)size;
}

static unsigned step(du_find *f)
{
    int id;
    struct search *s;
    char dir[256];
    const char *name;

    memcpy(&id, f->reserved, sizeof id);
    s = &searches[id];
    if (broken_dir != NULL && same(s->dir, broken_dir) && s->returned > 0)
        return DOSERR_FAIL;
    /* "." and ".." come first, but not in a root */
    if (s->name[0] == '\0' && strlen(s->dir) > 3 && s->next < 0) {
        fill(f, s->next == -2 ? "." : "..", _A_SUBDIR, 0);
        s->next++;
        s->returned++;
        return 0;
    }
    if (s->next < 0)
        s->next = 0;
    for (; disk[s->next].path != NULL; s->next++) {
        const struct entry *e = &disk[s->next];

        parent(dir, e->path);
        if (!same(dir, s->dir))
            continue;
        name = strrchr(e->path, '\\') + 1;
        if (s->name[0] && !same(name, s->name))
            continue;
        /* DOS: hidden, system and directory entries only if asked for;
         * a volume label only if asked for (some DOSes regardless) */
        if ((e->attrib & (_A_HIDDEN | _A_SYSTEM | _A_SUBDIR)) & ~s->attr)
            continue;
        fill(f, name, e->attrib, e->size);
        s->next++;
        s->returned++;
        return 0;
    }
    return s->returned == 0 && s->name[0] ? DOSERR_FILE_NOT_FOUND
                                          : DOSERR_NO_MORE_FILES;
}

static int dir_exists(const char *dir)
{
    int i;

    if (strlen(dir) == 3)
        return 1;
    for (i = 0; disk[i].path != NULL; i++)
        if (same(disk[i].path, dir) && (disk[i].attrib & _A_SUBDIR))
            return 1;
    return 0;
}

static unsigned mock_findfirst(const char *path, unsigned attr, du_find *f)
{
    struct search *s;
    const char *name;
    int id = nsearches++;

    if (id >= 256)
        abort();
    s = &searches[id];
    parent(s->dir, path);
    name = strrchr(path, '\\') + 1;
    strcpy(s->name, strcmp(name, "*.*") == 0 ? "" : name);
    s->attr = attr;
    s->next = -2;
    s->returned = 0;
    memcpy(f->reserved, &id, sizeof id);
    if (!dir_exists(s->dir))
        return DOSERR_PATH_NOT_FOUND;
    if (deny_dir != NULL && same(s->dir, deny_dir))
        return DOSERR_ACCESS_DENIED;
    return step(f);
}

static unsigned mock_findnext(du_find *f)
{
    return step(f);
}

static const struct du_ops ops = {
    mock_current, mock_cluster, mock_fullpath, mock_findfirst, mock_findnext
};

/* ------------------------------------------------------------------ */
/* running du */

static char outbuf[16384], errbuf[4096];
static int rc;

static void slurp(FILE *fp, char *buf, size_t size)
{
    size_t n;

    rewind(fp);
    n = fread(buf, 1, size - 1, fp);
    buf[n] = '\0';
    fclose(fp);
}

/* du with the arguments, up to NULL; output in outbuf and errbuf */
static void du(const char *a, ...)
{
    static char store[32][256];
    char *args[32];
    int n = 0;
    struct options o;
    FILE *out = tmpfile(), *err = tmpfile();
    va_list ap;

    args[n++] = strcpy(store[0], "du");
    va_start(ap, a);
    for (; a != NULL; a = va_arg(ap, const char *)) {
        strcpy(store[n], a);
        args[n] = store[n];
        n++;
    }
    va_end(ap);
    nsearches = 0;
    cluster_calls = 0;
    if (parse_options(n, args, &o, err) != OPT_RUN)
        rc = -1;
    else
        rc = du_run(&o, n, args, &ops, out, err);
    slurp(out, outbuf, sizeof outbuf);
    slurp(err, errbuf, sizeof errbuf);
}

static int out_is(const char *want)
{
    if (strcmp(outbuf, want) == 0)
        return 1;
    printf("--- want\n%s--- got\n%s---\n", want, outbuf);
    return 0;
}

static int err_has(const char *want)
{
    if (strstr(errbuf, want) != NULL)
        return 1;
    printf("--- stderr lacks: %s\n%s---\n", want, errbuf);
    return 0;
}

/* ------------------------------------------------------------------ */
/* disks */

#define D _A_SUBDIR
#define H _A_HIDDEN
#define S _A_SYSTEM

/* C: 2 KiB clusters; A: a 360 KB floppy's 1 KiB; D: 16 KiB */
static const struct entry tree[] = {
    { "C:\\VOLUME", _A_VOLID, 0 },
    { "C:\\ROOT.TXT", 0, 100 },
    { "C:\\EMPTY", D, 0 },
    { "C:\\TOP", D, 0 },
    { "C:\\TOP\\ZERO.DAT", 0, 0 },
    { "C:\\TOP\\EXACT.DAT", 0, 2048 },
    { "C:\\TOP\\OVER.DAT", 0, 2049 },
    { "C:\\TOP\\HIDDEN.SYS", H | S | _A_RDONLY, 1 },
    { "C:\\TOP\\MID", D | H, 0 },
    { "C:\\TOP\\MID\\LEAF", D, 0 },
    { "C:\\TOP\\MID\\LEAF\\BIG.BIN", 0, 10000 },
    { "C:\\TOP\\MID\\M.TXT", 0, 4096 },
    { "C:\\WORK", D, 0 },
    { "C:\\WORK\\W.TXT", 0, 3000 },
    { "A:\\FD.TXT", 0, 1500 },
    { "A:\\SUB", D, 0 },
    { "A:\\SUB\\ONE.TXT", 0, 1024 },
    { "D:\\BIG", D, 0 },
    { "D:\\BIG\\B1", 0, 4294967295UL },
    { "D:\\BIG\\B2", 0, 4294967295UL },
    { "D:\\SMALL", 0, 1 },
    { NULL, 0, 0 }
};

/* a directory chain deeper than DU_PATHMAX allows */
static struct entry deep[40];
static char deep_paths[40][300];

static void make_deep(void)
{
    int i;
    char p[200] = "C:";

    for (i = 0; i < 16; i++) {
        strcat(p, "\\DDDDDDDD");
        strcpy(deep_paths[i], p);
        deep[i].path = deep_paths[i];
        deep[i].attrib = D;
        deep[i].size = 0;
    }
    /* a file at each of two levels: one reachable, one too deep */
    strcat(strcpy(deep_paths[16], deep_paths[3]), "\\F.TXT");
    deep[16].path = deep_paths[16];
    deep[16].size = 1;
    strcat(strcpy(deep_paths[17], deep_paths[15]), "\\G.TXT");
    deep[17].path = deep_paths[17];
    deep[17].size = 1;
    deep[18].path = NULL;
}

static const struct entry emptydisk[] = {
    { NULL, 0, 0 }
};

static void reset(const struct entry *d)
{
    int i;

    disk = d;
    deny_dir = broken_dir = NULL;
    memset(clusters, 0, sizeof clusters);
    clusters[0] = 1024;
    clusters[2] = 2048;
    clusters[3] = 16384;
    for (i = 0; i < 26; i++)
        cwd[i][0] = '\0';
    current = 2;
}

int main(void)
{
    /* C:\ by default: directories after their contents, in directory
     * order; files rounded to 2 KiB clusters; hidden and system files
     * and directories counted; the volume label and "." and ".." not */
    reset(tree);
    du(NULL);
    CHECK(rc == RC_OK);
    CHECK(out_is(
        "        0  .\\EMPTY\n"
        "       10  .\\TOP\\MID\\LEAF\n"
        "       14  .\\TOP\\MID\n"
        "       22  .\\TOP\n"
        "        4  .\\WORK\n"
        "       28  .\n"));
    CHECK(errbuf[0] == '\0');

    /* the same, named; sizes: ROOT 2K, TOP: 0 + 2 + 4 + 2 (hidden) +
     * MID (10 + 4) = 22K, WORK 4K */
    du("C:\\", NULL);
    CHECK(rc == RC_OK);
    CHECK(out_is(
        "        0  C:\\EMPTY\n"
        "       10  C:\\TOP\\MID\\LEAF\n"
        "       14  C:\\TOP\\MID\n"
        "       22  C:\\TOP\n"
        "        4  C:\\WORK\n"
        "       28  C:\\\n"));

    /* the mock returns the volume label as some DOSes do: never shown */
    du("-a", "C:\\", NULL);
    CHECK(strstr(outbuf, "VOLUME") == NULL);
    CHECK(strstr(outbuf, "        2  C:\\ROOT.TXT\n") != NULL);

    /* -a: files too, each after its directory's earlier entries */
    du("-a", "c:\\top", NULL);
    CHECK(rc == RC_OK);
    CHECK(out_is(
        "        0  c:\\top\\ZERO.DAT\n"
        "        2  c:\\top\\EXACT.DAT\n"
        "        4  c:\\top\\OVER.DAT\n"
        "        2  c:\\top\\HIDDEN.SYS\n"
        "       10  c:\\top\\MID\\LEAF\\BIG.BIN\n"
        "       10  c:\\top\\MID\\LEAF\n"
        "        4  c:\\top\\MID\\M.TXT\n"
        "       14  c:\\top\\MID\n"
        "       22  c:\\top\n"));

    /* -b: the lengths, in bytes: 0 + 2048 + 2049 + 1 + 10000 + 4096 */
    du("-b", "C:\\TOP", NULL);
    CHECK(out_is(
        "    10000  C:\\TOP\\MID\\LEAF\n"
        "    14096  C:\\TOP\\MID\n"
        "    18194  C:\\TOP\n"));
    /* --apparent-size in KiB rounds the total up, not each file */
    du("--apparent-size", "-s", "C:\\TOP", NULL);
    CHECK(out_is("       18  C:\\TOP\n"));
    du("-bh", "-s", "C:\\TOP", NULL);
    CHECK(out_is("  17.8KiB  C:\\TOP\n"));
    /* -h: allocated */
    du("-hs", "C:\\TOP", NULL);
    CHECK(out_is("  22.0KiB  C:\\TOP\n"));

    /* -s: one line per operand */
    du("-s", "C:\\", "C:\\TOP\\MID", NULL);
    CHECK(rc == RC_OK);
    CHECK(out_is(
        "       28  C:\\\n"
        "       14  C:\\TOP\\MID\n"));

    /* display names keep the operand as given */
    du("-s", "c:top\\", "\\TOP\\.\\MID\\LEAF", "c:", NULL);
    CHECK(out_is(
        "       22  c:top\\\n"
        "       10  \\TOP\\.\\MID\\LEAF\n"
        "       28  c:\n"));
    du("c:top\\mid", NULL);
    CHECK(out_is(
        "       10  c:top\\mid\\LEAF\n"
        "       14  c:top\\mid\n"));
    du("C:", NULL);
    CHECK(strstr(outbuf, "       22  C:TOP\n") != NULL);
    /* relative to the current directory */
    strcpy(cwd[2], "\\TOP");
    du("-s", ".", "MID", "..\\WORK", NULL);
    CHECK(out_is(
        "       22  .\n"
        "       14  MID\n"
        "        4  ..\\WORK\n"));
    du("-a", "..", NULL);
    CHECK(strstr(outbuf, "        2  ..\\ROOT.TXT\n") != NULL);
    CHECK(strstr(outbuf, "       28  ..\n") != NULL);
    cwd[2][0] = '\0';

    /* a file operand: its own line, even with -s */
    du("C:\\TOP\\OVER.DAT", "-s", "c:\\top\\zero.dat", "C:\\TOP\\HIDDEN.SYS",
       NULL);
    CHECK(rc == RC_OK);
    CHECK(out_is(
        "        4  C:\\TOP\\OVER.DAT\n"
        "        0  c:\\top\\zero.dat\n"
        "        2  C:\\TOP\\HIDDEN.SYS\n"));
    du("-b", "C:\\TOP\\OVER.DAT", NULL);
    CHECK(out_is("     2049  C:\\TOP\\OVER.DAT\n"));
    /* a hidden directory operand */
    du("-s", "C:\\TOP\\MID", NULL);
    CHECK(out_is("       14  C:\\TOP\\MID\n"));

    /* an empty directory, and an empty root */
    du("C:\\EMPTY", NULL);
    CHECK(rc == RC_OK && out_is("        0  C:\\EMPTY\n"));
    reset(emptydisk);
    du("A:\\", NULL);
    CHECK(rc == RC_OK && out_is("        0  A:\\\n"));
    CHECK(errbuf[0] == '\0');

    /* -c: a grand total; the cluster size of each operand's drive */
    reset(tree);
    du("-c", "A:\\", "C:\\WORK", NULL);
    CHECK(rc == RC_OK);
    CHECK(out_is(
        "        1  A:\\SUB\n"
        "        3  A:\\\n"
        "        4  C:\\WORK\n"
        "        7  total\n"));
    /* each drive's cluster size is asked for once */
    du("-s", "C:\\TOP", "C:\\WORK", "A:\\SUB", "C:\\", NULL);
    CHECK(cluster_calls == 2);

    /* overlapping operands: shown in full, counted once in the total */
    du("-sc", "C:\\TOP\\MID", "C:\\", "C:\\TOP", NULL);
    CHECK(out_is(
        "       14  C:\\TOP\\MID\n"
        "       28  C:\\\n"
        "       22  C:\\TOP\n"
        "       28  total\n"));
    du("-sc", "C:\\TOP", "c:\\top", "C:\\TOP\\..\\TOP\\MID", NULL);
    CHECK(strstr(outbuf, "       22  total\n") != NULL);
    du("-sc", "C:\\TOP\\EXACT.DAT", "C:\\TOP", NULL);
    CHECK(strstr(outbuf, "       22  total\n") != NULL);
    du("-sc", "C:\\TOP", "C:\\WORK", "C:\\TOP\\EXACT.DAT", NULL);
    CHECK(strstr(outbuf, "       26  total\n") != NULL);
    /* C:\TOPX is not inside C:\TOP */
    CHECK(du_within("C:\\TOP\\X", "C:\\TOP"));
    CHECK(du_within("C:\\TOP", "c:\\top"));
    CHECK(du_within("C:\\TOP", "C:\\"));
    CHECK(!du_within("C:\\TOPX", "C:\\TOP"));
    CHECK(!du_within("C:\\TOP", "C:\\TOP\\X"));
    CHECK(!du_within("D:\\TOP", "C:\\"));
    CHECK(!du_within("C:\\", "C:\\TOP"));
    /* -c with one operand, and with none */
    du("-c", "-s", NULL);
    CHECK(out_is("       28  .\n       28  total\n"));

    /* totals over 4 GiB: two files of 4 GiB - 1 in 16 KiB clusters */
    du("-a", "D:\\", NULL);
    CHECK(out_is(
        "  4194304  D:\\BIG\\B1\n"
        "  4194304  D:\\BIG\\B2\n"
        "  8388608  D:\\BIG\n"
        "       16  D:\\SMALL\n"
        "  8388624  D:\\\n"));
    du("-b", "-s", "-c", "D:\\", "D:\\BIG", NULL);
    CHECK(out_is(
        "8589934591  D:\\\n"
        "8589934590  D:\\BIG\n"
        "8589934591  total\n"));
    du("-sh", "D:\\", NULL);
    CHECK(out_is("   8.1GiB  D:\\\n"));

    /* errors: an invalid drive, a missing path; the rest still reported */
    du("-s", "Q:\\", "C:\\WORK", "C:\\NOSUCH", "C:\\NO\\SUCH", "C:\\WORK", NULL);
    CHECK(rc == RC_FAIL);
    CHECK(out_is(
        "        4  C:\\WORK\n"
        "        4  C:\\WORK\n"));
    CHECK(err_has("du: Q:\\: drive Q: is invalid or not ready\n"));
    CHECK(err_has("du: C:\\NOSUCH: no such file or directory\n"));
    CHECK(err_has("du: C:\\NO\\SUCH: no such file or directory\n"));
    /* a failed operand is not in the total */
    du("-sc", "Q:", "C:\\WORK", NULL);
    CHECK(rc == RC_FAIL);
    CHECK(out_is("        4  C:\\WORK\n        4  total\n"));
    /* the current drive invalid */
    current = 16;
    du(NULL);
    CHECK(rc == RC_FAIL && out_is(""));
    CHECK(err_has("du: .: drive Q: is invalid or not ready\n"));
    current = 2;

    /* a directory that cannot be read: reported, the rest counted */
    deny_dir = "C:\\TOP\\MID";
    du("C:\\TOP", NULL);
    CHECK(rc == RC_FAIL);
    CHECK(out_is(
        "        0  C:\\TOP\\MID\n"
        "        8  C:\\TOP\n"));
    CHECK(err_has("du: C:\\TOP\\MID: access denied\n"));
    deny_dir = NULL;
    /* a search that fails part way */
    broken_dir = "C:\\TOP";
    du("-a", "C:\\TOP", NULL);
    CHECK(rc == RC_FAIL);
    CHECK(err_has("du: C:\\TOP: disk error\n"));
    CHECK(strstr(outbuf, "C:\\TOP\n") != NULL);
    broken_dir = NULL;

    /* too deep: reported and skipped, never overrun; the levels that
     * fit are counted */
    make_deep();
    reset(deep);
    du("-s", "C:\\", NULL);
    CHECK(rc == RC_FAIL);
    CHECK(out_is("        2  C:\\\n"));
    CHECK(err_has(": path too long\n"));
    CHECK(strlen(errbuf) < 1024);
    /* an operand that is too long itself */
    du("C:\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD"
       "\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD"
       "\\DDDDDDDD\\DDDDDDDD\\DDDDDDDD", NULL);
    CHECK(rc == RC_FAIL && out_is(""));
    CHECK(err_has(": path too long\n"));

    /* write errors */
    {
        struct options o;
        char *args[2];
        FILE *ro = fopen("/dev/null", "r"), *err = tmpfile();

        reset(tree);
        args[0] = (char *)"du";
        args[1] = (char *)"C:\\WORK";
        parse_options(2, args, &o, err);
        CHECK(du_run(&o, 2, args, &ops, ro, err) == RC_FATAL);
        slurp(err, errbuf, sizeof errbuf);
        CHECK(err_has("du: write error\n"));
        fclose(ro);
    }

    return UNIT_DONE();
}
