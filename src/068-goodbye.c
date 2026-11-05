/*
 * 068-goodbye.c (issue 1109) - see 068-goodbye.h
 *
 * For a general audience: the start's times are taken when the run
 * begins; at the end the receipt is printed into a temporary file beside
 * the receipt's name (made with mkstemp, so two runs never share one),
 * flushed to the disk, and renamed onto the name, which the system does
 * in one step.
 */
#include "068-goodbye.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#undef W_OK                           /* <unistd.h>'s access() mode (2) would hide 013-errors.h's W_OK (0) */

/* {{{ the run's state */
#define PATH_ROOM 600                 /* a receipt's path, and room left for the temporary name's ending */
typedef struct { char name[GOODBYE_NAME_MAX + 1]; double value; } GoodbyeCount;
struct Goodbye {
    char            receipt[PATH_ROOM];
    char            folder[PATH_ROOM];      /* the receipt's folder, where the temporary file goes */
    char           *map;
    time_t          started;          /* the machine's clock */
    struct timespec steady;           /* the steady clock, for the seconds */
    uint64_t        frames;
    int             written;
    GoodbyeCount    counts[GOODBYE_COUNTS_MAX];   /* the run's own counts (a headless run's: 089), in the order first given */
    int             ncounts;
};

static const char *const REASONS[GOODBYE_REASON_COUNT] = { "closed", "lost-connection", "error" };
/* }}} */

const char *goodbye_reason_name(GoodbyeReason reason)
{
    return (reason >= 0 && reason < GOODBYE_REASON_COUNT) ? REASONS[reason] : "(not a reason)";
}

void goodbye_utc(time_t t, char out[21])
{
    struct tm u;
    if (!gmtime_r(&t, &u) || strftime(out, 21, "%Y-%m-%dT%H:%M:%SZ", &u) != 20) snprintf(out, 21, "0000-00-00T00:00:00Z");
}

/* {{{ goodbye_begin, goodbye_free, goodbye_frame */
WErr goodbye_begin(const char *receipt, const char *map, Goodbye **out, WWhy *why)
{
    *out = NULL;
    if (!receipt || !*receipt) return w_fail(why, W_ERR_REFUSED, "the goodbye receipt has no path");
    if (strlen(receipt) >= PATH_ROOM - 20)
        return w_fail(why, W_ERR_REFUSED, "the goodbye receipt's path is longer than %d bytes: %.60s...", PATH_ROOM - 21, receipt);
    Goodbye *g = calloc(1, sizeof *g);
    if (!g || !(g->map = strdup(map ? map : ""))) { free(g); return w_fail(why, W_ERR_FULL, "no memory for the goodbye receipt"); }
    snprintf(g->receipt, sizeof g->receipt, "%s", receipt);
    const char *slash = strrchr(receipt, '/');
    if (!slash) snprintf(g->folder, sizeof g->folder, ".");
    else if (slash == receipt) snprintf(g->folder, sizeof g->folder, "/");
    else snprintf(g->folder, sizeof g->folder, "%.*s", (int)(slash - receipt), receipt);
    struct stat sb;
    if (stat(g->folder, &sb) || !S_ISDIR(sb.st_mode)) {
        WErr rc = w_fail(why, W_ERR_NOT_FOUND, "the goodbye receipt %s can't be written: its folder %s isn't there", receipt, g->folder);
        goodbye_free(g);
        return rc;
    }
    g->started = time(NULL);
    clock_gettime(CLOCK_MONOTONIC, &g->steady);
    *out = g;
    return W_OK;
}

void goodbye_free(Goodbye *g)
{
    if (!g) return;
    free(g->map);
    free(g);
}

void goodbye_frame(Goodbye *g)
{
    if (g) ++g->frames;
}
/* }}} */

/* {{{ goodbye_count */
static const char *const OWN_KEYS[] = { "format", "map", "started", "ended", "seconds", "frames", "reason", "why", NULL };
WErr goodbye_count(Goodbye *g, const char *name, double value, WWhy *why)
{
    if (!g) return W_OK;
    size_t n = name ? strlen(name) : 0;
    int good = n > 0 && n <= GOODBYE_NAME_MAX && name[0] >= 'a' && name[0] <= 'z';
    for (size_t i = 0; good && i < n; ++i) good = (name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '_';
    if (!good) return w_fail(why, W_ERR_REFUSED, "a receipt's count is named in small letters, digits and _, a letter first, at most %d characters; \"%.40s\" isn't that", GOODBYE_NAME_MAX, name ? name : "");
    for (int k = 0; OWN_KEYS[k]; ++k) if (!strcmp(name, OWN_KEYS[k])) return w_fail(why, W_ERR_REFUSED, "\"%s\" is the receipt's own field, not a count", name);
    if (!isfinite(value)) return w_fail(why, W_ERR_REFUSED, "the receipt's count %s isn't a finite number", name);
    if (g->written) return w_fail(why, W_ERR_REFUSED, "the goodbye receipt %s was written already", g->receipt);
    for (int k = 0; k < g->ncounts; ++k) if (!strcmp(g->counts[k].name, name)) { g->counts[k].value = value; return W_OK; }
    if (g->ncounts == GOODBYE_COUNTS_MAX) return w_fail(why, W_ERR_FULL, "the receipt holds %d counts at most; %s is one more", GOODBYE_COUNTS_MAX, name);
    snprintf(g->counts[g->ncounts].name, sizeof g->counts[g->ncounts].name, "%s", name);
    g->counts[g->ncounts++].value = value;
    return W_OK;
}
/* }}} */

/* {{{ static lua_string: `s` as a Lua string in double quotes: a backslash,
 * a quote and every control byte escaped (\n, \r, \t, else three digits,
 * so a digit after it can't join it); other bytes as they are */
static void lua_string(FILE *f, const char *s)
{
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p == '\\' || *p == '"') { fputc('\\', f); fputc(*p, f); }
        else if (*p == '\n') fputs("\\n", f);
        else if (*p == '\r') fputs("\\r", f);
        else if (*p == '\t') fputs("\\t", f);
        else if (*p < ' ' || *p == 127) fprintf(f, "\\%03d", *p);
        else fputc(*p, f);
    }
    fputc('"', f);
}
/* }}} */

/* {{{ goodbye_end */
WErr goodbye_end(Goodbye *g, GoodbyeReason reason, const char *why_text, WWhy *why)
{
    if (!g) return W_OK;
    if (g->written) return w_fail(why, W_ERR_REFUSED, "the goodbye receipt %s was written already", g->receipt);
    if (reason < 0 || reason >= GOODBYE_REASON_COUNT) return w_fail(why, W_ERR_REFUSED, "%d isn't a reason a run ends for", (int)reason);
    g->written = 1;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double seconds = (double)(now.tv_sec - g->steady.tv_sec) + (double)(now.tv_nsec - g->steady.tv_nsec) / 1e9;
    char started[21], ended[21];
    goodbye_utc(g->started, started);
    goodbye_utc(time(NULL), ended);

    const char *base = strrchr(g->receipt, '/');
    base = base ? base + 1 : g->receipt;
    char temp[2 * PATH_ROOM + 16];                         /* the folder and the name are parts of the receipt's path, so it fits */
    snprintf(temp, sizeof temp, "%s/.%s.XXXXXX", g->folder, base);
    int fd = mkstemp(temp);
    if (fd < 0) return w_fail(why, W_ERR_IO, "the goodbye receipt %s wasn't written: a temporary file in %s couldn't be made (%s)", g->receipt, g->folder, strerror(errno));
    fchmod(fd, 0644);                                       /* an ordinary file's rights, not mkstemp's owner-only ones */
    FILE *f = fdopen(fd, "w");
    if (!f) {
        int e = errno;
        close(fd); unlink(temp);
        return w_fail(why, W_ERR_IO, "the goodbye receipt %s wasn't written: %s couldn't be opened (%s)", g->receipt, temp, strerror(e));
    }
    fprintf(f, "-- the W client's goodbye receipt (issue 1109): the editor's Test Map reads it back\n");
    fprintf(f, "return {\n    format  = 1,\n    map     = ");
    lua_string(f, g->map);
    fprintf(f, ",\n    started = \"%s\",\n    ended   = \"%s\",\n    seconds = %.3f,\n    frames  = %llu,\n    reason  = \"%s\",\n",
            started, ended, seconds < 0 ? 0.0 : seconds, (unsigned long long)g->frames, REASONS[reason]);
    if (reason == GOODBYE_ERROR) {
        fprintf(f, "    why     = ");
        lua_string(f, why_text && *why_text ? why_text : "the client stopped on an error, with no sentence saying what it was");
        fprintf(f, ",\n");
    }
    /* the run's own counts, each exactly (17 digits read back as the same double) */
    for (int k = 0; k < g->ncounts; ++k) fprintf(f, "    %s = %.17g,\n", g->counts[k].name, g->counts[k].value == 0 ? 0.0 : g->counts[k].value);
    fprintf(f, "}\n");
    int bad = fflush(f) || ferror(f) || fsync(fileno(f));
    int e = errno;
    if (fclose(f) && !bad) { bad = 1; e = errno; }
    if (bad) {
        unlink(temp);
        return w_fail(why, W_ERR_IO, "the goodbye receipt %s wasn't written: writing %s failed (%s)", g->receipt, temp, strerror(e));
    }
    if (rename(temp, g->receipt)) {
        e = errno;
        unlink(temp);
        return w_fail(why, W_ERR_IO, "the goodbye receipt %s wasn't written: the finished %s couldn't be renamed onto it (%s)", g->receipt, temp, strerror(e));
    }
    return W_OK;
}
/* }}} */
