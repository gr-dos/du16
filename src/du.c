#include <stdlib.h>
#include <string.h>
#include "du.h"
#include "out.h"

/* Space counted for a file or a tree. */
struct tot {
    dusize_t alloc;         /* allocated: rounded up to clusters */
    dusize_t bytes;         /* logical: the files' lengths */
};

/* An operand, for the grand total. */
struct seen {
    char *path;             /* absolute; NULL if it could not be read */
    struct tot t;
};

struct walk {
    const struct options *o;
    const struct du_ops *ops;
    FILE *out, *err;
    const char *arg;        /* the operand as given, for display */
    unsigned base;          /* length of the operand's absolute path */
    unsigned long cluster;  /* bytes per cluster on its drive */
    int rc;
};

/* The path being walked, absolute, and a search record for each level:
 * static, so the depth of a walk costs little stack. */
static char path[DU_PATHMAX];
static du_find finds[DU_MAXDEPTH];

static void add(struct tot *t, const struct tot *u)
{
    t->alloc += u->alloc;
    t->bytes += u->bytes;
}

static int is_sep(int c)
{
    return c == '\\' || c == '/' || c == ':';
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int du_within(const char *a, const char *b)
{
    size_t i, n = strlen(b);

    if (n == 0)
        return 0;
    for (i = 0; i < n; i++)
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i]))
            return 0;
    return a[n] == '\0' || a[n] == '\\' || b[n - 1] == '\\';
}

/* Writes the display name of path: the operand as given, then the part
 * of path below the operand, if any. */
static void put_name(struct walk *w, FILE *fp)
{
    const char *rel = path + w->base;
    size_t n = strlen(w->arg);

    fputs(w->arg, fp);
    if (*rel == '\0')
        return;
    if (*rel == '\\')
        rel++;
    if (n > 0 && !is_sep(w->arg[n - 1]))
        fputc('\\', fp);
    fputs(rel, fp);
}

static void line(struct walk *w, const struct tot *t)
{
    char size[24];

    fmt_size(size, w->o->apparent ? t->bytes : t->alloc, w->o->unit);
    put(w->out, size, "  ", NULL);
    put_name(w, w->out);
    fputc('\n', w->out);
}

static const char *dos_error(unsigned e, char *buf)
{
    switch (e) {
    case DOSERR_FILE_NOT_FOUND:
        return "file not found";
    case DOSERR_PATH_NOT_FOUND:
        return "path not found";
    case DOSERR_ACCESS_DENIED:
        return "access denied";
    case DOSERR_INVALID_DRIVE:
        return "invalid drive";
    case DOSERR_NOT_READY:
        return "drive not ready";
    case DOSERR_FAIL:
        return "disk error";
    }
    strcpy(buf, "DOS error ");
    fmt_u64(buf + strlen(buf), e);
    return buf;
}

/* A diagnostic about path, which the walk does not count in full. */
static void fail(struct walk *w, const char *why)
{
    fputs("du: ", w->err);
    put_name(w, w->err);
    put(w->err, ": ", why, "\n", NULL);
    w->rc = RC_FAIL;
}

static void file(struct walk *w, const du_find *f, struct tot *t)
{
    unsigned long size = (unsigned long)f->size;

    t->alloc = du_alloc(size, w->cluster);
    t->bytes = size;
}

static int dots(const char *name)
{
    return name[0] == '.' &&
        (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

/*
 * Adds to t the files of the directory path[0..len), and of the
 * directories below it, writing a line for each directory (and, with
 * -a, each file) after its contents, as Unix du does.  path is as it was
 * on return.
 */
static void walk(struct walk *w, unsigned len, unsigned depth, struct tot *t)
{
    du_find *f = &finds[depth];
    struct tot u;
    unsigned sub = len, n, e;
    char buf[24];

    if (path[len - 1] != '\\')
        path[sub++] = '\\';
    /* room for "*.*" and the terminator */
    if (depth >= DU_MAXDEPTH || sub + 4 > DU_PATHMAX) {
        path[len] = '\0';
        fail(w, "path too long");
        return;
    }
    strcpy(path + sub, "*.*");
    e = w->ops->findfirst(path, DU_ATTRS, f);
    for (; e == 0; e = w->ops->findnext(f)) {
        if ((f->attrib & _A_VOLID) || dots(f->name))
            continue;
        n = (unsigned)strlen(f->name);
        if (sub + n + 1 > DU_PATHMAX) {
            path[sub] = '\0';
            fputs("du: ", w->err);
            put_name(w, w->err);
            put(w->err, f->name, ": path too long\n", NULL);
            w->rc = RC_FAIL;
            continue;
        }
        memcpy(path + sub, f->name, n + 1);
        u.alloc = u.bytes = 0;
        if (f->attrib & _A_SUBDIR) {
            walk(w, sub + n, depth + 1, &u);
            if (!w->o->summary)
                line(w, &u);
        } else {
            file(w, f, &u);
            if (w->o->all)
                line(w, &u);
        }
        add(t, &u);
    }
    path[len] = '\0';
    /* an empty root has no "." entry, so its search finds nothing */
    if (e != DOSERR_NO_MORE_FILES && e != DOSERR_FILE_NOT_FOUND)
        fail(w, dos_error(e, buf));
}

/* Reports one operand.  Returns 0 if it was found (the walk may still
 * have failed in part), with its total in t. */
static int operand(struct walk *w, unsigned long *clusters, struct tot *t)
{
    int drive = operand_drive(w->arg);
    unsigned e;
    char letter[2], buf[24];

    t->alloc = t->bytes = 0;
    if (drive == DRIVE_CURRENT)
        drive = w->ops->current_drive();
    if (drive < 0 || drive > 25) {
        put(w->err, "du: ", w->arg, ": invalid drive\n", NULL);
        w->rc = RC_FAIL;
        return -1;
    }
    if (clusters[drive] == 0)
        clusters[drive] = w->ops->cluster_size(drive);
    if (clusters[drive] == 0) {
        letter[0] = (char)('A' + drive);
        letter[1] = '\0';
        put(w->err, "du: ", w->arg, ": drive ", letter,
            ": is invalid or not ready\n", NULL);
        w->rc = RC_FAIL;
        return -1;
    }
    w->cluster = clusters[drive];
    if (w->ops->fullpath(path, w->arg, DU_PATHMAX) == NULL) {
        put(w->err, "du: ", w->arg, ": path too long\n", NULL);
        w->rc = RC_FAIL;
        return -1;
    }
    w->base = (unsigned)strlen(path);
    /* a root is not in any directory, so it cannot be searched for */
    if (w->base != 3) {
        e = w->ops->findfirst(path, DU_ATTRS, &finds[0]);
        if (e == DOSERR_FILE_NOT_FOUND || e == DOSERR_PATH_NOT_FOUND ||
            e == DOSERR_NO_MORE_FILES) {
            put(w->err, "du: ", w->arg, ": no such file or directory\n",
                NULL);
            w->rc = RC_FAIL;
            return -1;
        }
        if (e != 0) {
            put(w->err, "du: ", w->arg, ": ", dos_error(e, buf), "\n", NULL);
            w->rc = RC_FAIL;
            return -1;
        }
        if (!(finds[0].attrib & _A_SUBDIR)) {
            file(w, &finds[0], t);
            line(w, t);
            return 0;
        }
    }
    walk(w, w->base, 0, t);
    line(w, t);
    return 0;
}

/* The grand total: each operand counts once, and not if it is inside
 * another one (an earlier one, if they are the same). */
static void grand_total(struct walk *w, const struct seen *s, int n)
{
    struct tot t;
    int i, j, inside;
    char size[24];

    t.alloc = t.bytes = 0;
    for (i = 0; i < n; i++) {
        if (s[i].path == NULL)
            continue;
        inside = 0;
        for (j = 0; j < n && !inside; j++)
            if (j != i && s[j].path != NULL &&
                du_within(s[i].path, s[j].path) &&
                (j < i || !du_within(s[j].path, s[i].path)))
                inside = 1;
        if (!inside)
            add(&t, &s[i].t);
    }
    fmt_size(size, w->o->apparent ? t.bytes : t.alloc, w->o->unit);
    put(w->out, size, "  total\n", NULL);
}

int du_run(const struct options *o, int argc, char **argv,
           const struct du_ops *ops, FILE *out, FILE *err)
{
    static char dot[] = ".";
    static char *here[2] = { NULL, dot };
    unsigned long clusters[26];
    struct walk w;
    struct seen *s = NULL;
    int i, n = 0, operands = 0;

    w.o = o;
    w.ops = ops;
    w.out = out;
    w.err = err;
    w.rc = RC_OK;
    memset(clusters, 0, sizeof clusters);

    for (i = 1; i < argc; i++)
        if (argv[i] != NULL)
            operands++;
    if (operands == 0) {
        argv = here;
        argc = 2;
        operands = 1;
    }
    if (o->total) {
        s = (struct seen *)malloc(sizeof *s * (unsigned)operands);
        if (s == NULL) {
            fputs("du: out of memory\n", err);
            return RC_FATAL;
        }
    }

    for (i = 1; i < argc; i++) {
        if (argv[i] == NULL)
            continue;
        w.arg = argv[i];
        if (s == NULL) {
            struct tot t;
            operand(&w, clusters, &t);
            continue;
        }
        s[n].path = NULL;
        if (operand(&w, clusters, &s[n].t) == 0) {
            s[n].path = (char *)malloc(strlen(path) + 1);
            if (s[n].path == NULL) {
                fputs("du: out of memory\n", err);
                return RC_FATAL;
            }
            strcpy(s[n].path, path);
        }
        n++;
    }
    if (s != NULL)
        grand_total(&w, s, n);

    if (fflush(out) != 0 || ferror(out)) {
        fputs("du: write error\n", err);
        return RC_FATAL;
    }
    return w.rc;
}
