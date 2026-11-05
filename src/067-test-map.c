/*
 * 067-test-map.c (issue 1109) - see 067-test-map.h
 *
 * For a general audience: one pass over the arguments reads the test
 * map's six options and checks the client's own against their list,
 * keeping the first thing wrong; the pass goes on after it, so that a
 * receipt named later on the line can still be written to say what was
 * wrong. Then the run is decided: a test-map run or not, and good or not.
 */
#include "067-test-map.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the test map's state */
struct TestMap {
    char   folder[600];               /* --test-map, as given */
    char   receipt[600];              /* --receipt, as given */
    int    camera;                    /* 1: --camera was given */
    double camera_x, camera_y;
    char   host[256];                 /* --realm's host and port */
    int    port;
    char   account[64];               /* --account; "" when none */
    char   password[256];             /* W_CLIENT_PASSWORD's value; wiped when freed */
    char   headless[600];             /* --headless's moves file; "" for a run with a window */
};

/* the test map's own options, each taking one value */
enum { OPT_TEST_MAP, OPT_RECEIPT, OPT_CAMERA, OPT_REALM, OPT_ACCOUNT, OPT_HEADLESS, OPT_COUNT };
static const char *const OWN[OPT_COUNT] = { "--test-map", "--receipt", "--camera", "--realm", "--account", "--headless" };
/* }}} */

/* {{{ static the values' readers: each fills why and returns its code on
 * a value it refuses */
static int plain_text(const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) if (*p <= ' ' || *p == 127) return 0;
    return 1;
}
static WErr read_camera(TestMap *tm, const char *v, WWhy *why)
{
    char *end;
    errno = 0;
    double x = strtod(v, &end);                                 /* strtod passes over spaces before a number: none are taken */
    int good = plain_text(v) && end != v && *end == ',' && !errno && isfinite(x);
    const char *ys = good ? end + 1 : v;
    double y = good ? strtod(ys, &end) : 0;
    good = good && end != ys && *end == '\0' && !errno && isfinite(y);
    if (!good) return w_fail(why, W_ERR_REFUSED, "--camera takes the world's x and y in yards with a comma between (12.5,-300); \"%s\" isn't that", v);
    tm->camera = 1; tm->camera_x = x; tm->camera_y = y;
    return W_OK;
}
static WErr read_realm(TestMap *tm, const char *v, WWhy *why)
{
    const char *colon = strrchr(v, ':');
    char *end = NULL;
    long port = colon ? strtol(colon + 1, &end, 10) : 0;
    size_t host_len = colon ? (size_t)(colon - v) : 0;
    if (!colon || host_len == 0 || host_len >= sizeof tm->host || !end || end == colon + 1 || *end || port < 1 || port > 65535 || !plain_text(v))
        return w_fail(why, W_ERR_REFUSED, "--realm takes the login server as host:port, a port from 1 to 65535 (127.0.0.1:3724); \"%s\" isn't that", v);
    memcpy(tm->host, v, host_len);
    tm->host[host_len] = '\0';
    tm->port = (int)port;
    return W_OK;
}
static WErr read_account(TestMap *tm, const char *v, WWhy *why)
{
    if (!*v || strlen(v) >= sizeof tm->account || !plain_text(v))
        return w_fail(why, W_ERR_REFUSED, "--account takes an account name of 1 to %d characters, with no spaces; \"%s\" isn't that", (int)sizeof tm->account - 1, v);
    snprintf(tm->account, sizeof tm->account, "%s", v);
    return W_OK;
}
static WErr read_path(char *into, size_t n, const char *option, const char *v, WWhy *why)
{
    if (strlen(v) >= n) return w_fail(why, W_ERR_REFUSED, "%s's path is longer than %d bytes: %.60s...", option, (int)n - 1, v);
    snprintf(into, n, "%s", v);
    return W_OK;
}
/* }}} */

/* {{{ test_map_parse */
/* a value is missing when the arguments run out or the next one is an option */
static int missing(int argc, char *const *argv, int i) { return i >= argc || !strncmp(argv[i], "--", 2); }

WErr test_map_parse(int argc, char *const *argv, const ClientOption *others, const char *password, TestMap **out, WWhy *why)
{
    *out = NULL;
    TestMap *tm = calloc(1, sizeof *tm);
    if (!tm) return w_fail(why, W_ERR_FULL, "no memory for the test map's options");
    snprintf(tm->host, sizeof tm->host, "%s", TEST_MAP_REALM_DEFAULT_HOST);
    tm->port = TEST_MAP_REALM_DEFAULT_PORT;
    WWhy wrong = { W_OK, "" };                                  /* the first thing wrong; later ones are passed over */
    int seen[OPT_COUNT] = { 0 };
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        int own = -1;
        for (int k = 0; k < OPT_COUNT; ++k) if (!strcmp(a, OWN[k])) own = k;
        if (own >= 0) {
            if (seen[own]++) { if (!wrong.code) w_fail(&wrong, W_ERR_REFUSED, "%s was given twice", a); }
            if (missing(argc, argv, i + 1)) {
                if (!wrong.code) w_fail(&wrong, W_ERR_REFUSED, "%s needs a value after it", a);
                if (own == OPT_RECEIPT) seen[own] = 0;          /* no receipt to write to */
                continue;
            }
            const char *v = argv[++i];
            if (seen[own] > 1) continue;                        /* the first one stands */
            WWhy w = { W_OK, "" };
            WErr rc = W_OK;
            switch (own) {
            case OPT_TEST_MAP: rc = read_path(tm->folder, sizeof tm->folder, a, v, &w); break;
            case OPT_RECEIPT:  rc = read_path(tm->receipt, sizeof tm->receipt, a, v, &w); if (rc) seen[own] = 0; break;
            case OPT_CAMERA:   rc = read_camera(tm, v, &w); break;
            case OPT_REALM:    rc = read_realm(tm, v, &w); break;
            case OPT_ACCOUNT:  rc = read_account(tm, v, &w); break;
            case OPT_HEADLESS: rc = read_path(tm->headless, sizeof tm->headless, a, v, &w); break;
            }
            if (rc && !wrong.code) wrong = w;
            continue;
        }
        const ClientOption *o = NULL;
        for (const ClientOption *c = others; c && c->name; ++c) if (!strcmp(a, c->name)) o = c;
        if (o) {
            int have = 0;
            while (have < o->values && !missing(argc, argv, i + 1 + have)) ++have;
            if (have < o->values && !wrong.code)
                w_fail(&wrong, W_ERR_REFUSED, "%s needs %d value%s after it, and has %d", a, o->values, o->values == 1 ? "" : "s", have);
            i += have;
            continue;
        }
        if (!wrong.code) {
            if (!strncmp(a, "--", 2)) w_fail(&wrong, W_ERR_REFUSED, "the option %s isn't one this client takes", a);
            else w_fail(&wrong, W_ERR_REFUSED, "the argument \"%s\" follows no option that takes it", a);
        }
    }
    /* {{{ not a test-map run: the test map's other options don't belong */
    if (!seen[OPT_TEST_MAP]) {
        int stray = -1;
        for (int k = 0; k < OPT_COUNT; ++k) if (seen[k] && stray < 0) stray = k;
        if (!wrong.code && stray >= 0) w_fail(&wrong, W_ERR_REFUSED, "%s is for a --test-map run, and none was asked for", OWN[stray]);
        test_map_free(tm);
        if (wrong.code) { *why = wrong; return wrong.code; }
        return W_OK;
    }
    /* }}} */
    /* {{{ a test-map run: it needs a receipt and a password */
    if (!seen[OPT_RECEIPT]) {
        if (!wrong.code) w_fail(&wrong, W_ERR_NOT_FOUND, "a --test-map run needs --receipt FILE: where the goodbye receipt goes for the editor to read");
        test_map_free(tm);
        *why = wrong;
        return wrong.code;
    }
    if (!wrong.code && (!password || !*password))
        w_fail(&wrong, W_ERR_NOT_FOUND, "a --test-map run reads the account's password from %s, and it is %s (it is never taken on the command line, where every user of this machine can read it)",
               TEST_MAP_PASSWORD_VARIABLE, password ? "empty" : "not set");
    else if (!wrong.code && strlen(password) >= sizeof tm->password)
        w_fail(&wrong, W_ERR_REFUSED, "%s is longer than %d characters", TEST_MAP_PASSWORD_VARIABLE, (int)sizeof tm->password - 1);
    else if (!wrong.code) snprintf(tm->password, sizeof tm->password, "%s", password);
    /* }}} */
    *out = tm;
    if (wrong.code) { *why = wrong; return wrong.code; }
    return W_OK;
}
/* }}} */

/* {{{ test_map_free and the readers */
void test_map_free(TestMap *tm)
{
    if (!tm) return;
    volatile char *p = tm->password;                            /* wiped, so freed memory doesn't hold it */
    for (size_t i = 0; i < sizeof tm->password; ++i) p[i] = 0;
    free(tm);
}
const char *test_map_folder(const TestMap *tm)     { return tm->folder; }
const char *test_map_receipt(const TestMap *tm)    { return tm->receipt; }
const char *test_map_realm_host(const TestMap *tm) { return tm->host; }
int         test_map_realm_port(const TestMap *tm) { return tm->port; }
const char *test_map_account(const TestMap *tm)    { return tm->account; }
const char *test_map_headless(const TestMap *tm)   { return tm->headless; }
const char *test_map_password(const TestMap *tm)   { return tm->password; }
int test_map_camera(const TestMap *tm, double *x, double *y)
{
    if (tm->camera) { *x = tm->camera_x; *y = tm->camera_y; }
    return tm->camera;
}
/* }}} */

/* {{{ test_map_place */
WErr test_map_place(const TestMap *tm, const Settings *st, const char *assets_dir, MpqChain *chain, ResolverPlace *out, WWhy *why)
{
    if (!tm->folder[0]) return w_fail(why, W_ERR_REFUSED, "the test map has no folder (--test-map was given without one)");
    if (!assets_dir && (settings_text(st, "assetPacks")[0] || settings_text(st, "assetOverlays")[0]))
        return w_fail(why, W_ERR_REFUSED, "the settings name packs (\"%s\") or overlays (\"%s\"), but no --assets folder was given to find them under",
                      settings_text(st, "assetPacks"), settings_text(st, "assetOverlays"));
    WErr rc = resolver_place_from_settings(st, assets_dir ? assets_dir : "", chain, out, why);
    if (rc) return rc;
    if (out->noverlays == RESOLVER_FOLDERS_MAX)
        return w_fail(why, W_ERR_FULL, "the settings name %d overlays already, the most there can be, so the test map %s has no room in front of them", RESOLVER_FOLDERS_MAX, tm->folder);
    memmove(out->overlays[1], out->overlays[0], sizeof out->overlays[0] * (size_t)out->noverlays);
    snprintf(out->overlays[0], sizeof out->overlays[0], "%s", tm->folder);
    ++out->noverlays;
    return W_OK;
}
/* }}} */
