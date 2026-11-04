/*
 * 065-xml.c (issue 702) - see 065-xml.h.
 *
 * One pass over the bytes, never backwards. Each event's strings are
 * built in one buffer (names and values end in a zero byte) and pointed
 * at once the event is whole, since the buffer may move while it grows.
 * The open elements' names sit in a second buffer, for matching their
 * closing tags.
 */
#include "065-xml.h"
#include "013-errors.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t off;                           /* its name, in `stk` */
    int line;
} Open;

struct XmlReader {
    const unsigned char *s;
    size_t n, i;
    int line;
    size_t line_start;
    char *buf;                            /* the current event's strings */
    size_t blen, bcap;
    size_t *aoff;                         /* name and value offsets in `buf`, two a pair */
    XmlAttr *attrs;
    size_t attrs_cap;
    int na, acap;
    char *stk;                            /* the open elements' names, each ending in a zero byte */
    size_t slen, scap;
    Open *open;
    int depth, ocap;
    int pending_end;                      /* the last START was <a/> */
    int seen_root, root_closed;
    int failed;
    WWhy err;
};

/* {{{ static grow: room for `more` bytes past `len` in a buffer of `cap` */
static int grow(void **p, size_t *cap, size_t len, size_t more, size_t unit)
{
    if (len + more <= *cap) return 1;
    size_t c = *cap ? *cap : 64;
    while (c < len + more) c *= 2;
    void *q = realloc(*p, c * unit);
    if (!q) return 0;
    *p = q;
    *cap = c;
    return 1;
}
/* }}} */

/* {{{ static fail: the file's fault at the reader's place (or at `line`,
 * `column` when given), kept so every later call says the same */
static WErr fail_at(XmlReader *x, WWhy *why, WErr code, int line, int column, const char *format, ...)
    __attribute__((format(printf, 6, 7)));
static WErr fail_at(XmlReader *x, WWhy *why, WErr code, int line, int column, const char *format, ...)
{
    char what[480];
    va_list ap;
    va_start(ap, format);
    vsnprintf(what, sizeof what, format, ap);
    va_end(ap);
    if (code == W_ERR_FULL) w_fail(&x->err, code, "no memory left to read the XML (at line %d)", x->line);
    else w_fail(&x->err, code, "line %d, column %d: %s", line, column, what);
    x->failed = 1;
    *why = x->err;
    return code;
}
#define FAIL(code, ...) fail_at(x, why, (code), x->line, (int)(x->i - x->line_start) + 1, __VA_ARGS__)
#define NOMEM() fail_at(x, why, W_ERR_FULL, 0, 0, "%s", "")
/* }}} */

/* {{{ static put, putc_: onto the event's buffer; 0 when out of memory */
static int put(XmlReader *x, const void *p, size_t n)
{
    if (!grow((void **)&x->buf, &x->bcap, x->blen, n + 1, 1)) return 0;
    memcpy(x->buf + x->blen, p, n);
    x->blen += n;
    return 1;
}
static int putc_(XmlReader *x, char c) { return put(x, &c, 1); }
/* }}} */

/* {{{ static step: past one byte, counting lines (\n, \r\n, a lone \r) */
static void step(XmlReader *x)
{
    unsigned char c = x->s[x->i++];
    if (c == '\n' || (c == '\r' && (x->i >= x->n || x->s[x->i] != '\n'))) { ++x->line; x->line_start = x->i; }
}
static int at(const XmlReader *x, const char *lit) { size_t k = strlen(lit); return x->n - x->i >= k && !memcmp(x->s + x->i, lit, k); }
/* past `end`; 0 when the file ends first */
static int skip_past(XmlReader *x, const char *end)
{
    while (x->i < x->n) {
        if (at(x, end)) { for (size_t k = strlen(end); k; --k) step(x); return 1; }
        step(x);
    }
    return 0;
}
static int is_space(unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static void skip_spaces(XmlReader *x) { while (x->i < x->n && is_space(x->s[x->i])) step(x); }
static int name_start(unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':' || c >= 0x80; }
static int name_char(unsigned char c) { return name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.'; }
/* }}} */

/* {{{ xml_reader_new, xml_reader_free */
XmlReader *xml_reader_new(const char *data, size_t len, WWhy *why)
{
    XmlReader *x = calloc(1, sizeof *x);
    if (!x) { w_fail(why, W_ERR_FULL, "no memory for an XML reader"); return NULL; }
    x->s = (const unsigned char *)data;
    x->n = data ? len : 0;
    x->line = 1;
    if (x->n >= 3 && !memcmp(x->s, "\xEF\xBB\xBF", 3)) x->i = x->line_start = 3;   /* UTF-8's byte-order mark */
    return x;
}
void xml_reader_free(XmlReader *x)
{
    if (!x) return;
    free(x->buf); free(x->aoff); free(x->attrs); free(x->stk); free(x->open);
    free(x);
}
/* }}} */

/* {{{ static entity: at an &, writes what it stands for onto the buffer */
static WErr entity(XmlReader *x, WWhy *why)
{
    size_t k = x->i + 1;
    while (k < x->n && k - x->i <= 12 && x->s[k] != ';' && x->s[k] != '&' && x->s[k] != '<' && !is_space(x->s[k])) ++k;
    if (k >= x->n || x->s[k] != ';' || k == x->i + 1)
        return FAIL(W_ERR_CORRUPT, "an & that doesn't begin an entity (an & on its own is written &amp;)");
    const char *e = (const char *)x->s + x->i + 1;
    size_t elen = k - x->i - 1;
    static const struct { const char *name; char c; } NAMED[] = {
        { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' },
    };
    int ok = 0;
    for (size_t j = 0; j < sizeof NAMED / sizeof NAMED[0]; ++j)
        if (strlen(NAMED[j].name) == elen && !memcmp(NAMED[j].name, e, elen)) { if (!putc_(x, NAMED[j].c)) return NOMEM(); ok = 1; }
    if (!ok && e[0] == '#') {
        int hex = elen > 1 && e[1] == 'x';
        uint32_t cp = 0;
        size_t j = hex ? 2 : 1;
        if (j >= elen) return FAIL(W_ERR_CORRUPT, "the entity &%.*s; has no number", (int)elen, e);
        for (; j < elen; ++j) {
            unsigned char c = (unsigned char)e[j];
            int d = c >= '0' && c <= '9' ? c - '0' : hex && c >= 'a' && c <= 'f' ? c - 'a' + 10 : hex && c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) return FAIL(W_ERR_CORRUPT, "the entity &%.*s; isn't a number", (int)elen, e);
            cp = cp * (hex ? 16 : 10) + (uint32_t)d;
            if (cp > 0x10FFFF) break;
        }
        if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return FAIL(W_ERR_CORRUPT, "the entity &%.*s; names no character", (int)elen, e);
        unsigned char u[4];
        size_t un;
        if (cp < 0x80) { u[0] = (unsigned char)cp; un = 1; }
        else if (cp < 0x800) { u[0] = (unsigned char)(0xC0 | cp >> 6); u[1] = (unsigned char)(0x80 | (cp & 0x3F)); un = 2; }
        else if (cp < 0x10000) { u[0] = (unsigned char)(0xE0 | cp >> 12); u[1] = (unsigned char)(0x80 | (cp >> 6 & 0x3F)); u[2] = (unsigned char)(0x80 | (cp & 0x3F)); un = 3; }
        else { u[0] = (unsigned char)(0xF0 | cp >> 18); u[1] = (unsigned char)(0x80 | (cp >> 12 & 0x3F)); u[2] = (unsigned char)(0x80 | (cp >> 6 & 0x3F)); u[3] = (unsigned char)(0x80 | (cp & 0x3F)); un = 4; }
        if (!put(x, u, un)) return NOMEM();
        ok = 1;
    }
    if (!ok) return FAIL(W_ERR_CORRUPT, "the entity &%.*s; isn't one of XML's five (amp, lt, gt, quot, apos) nor a number", (int)elen, e);
    while (x->i <= k) step(x);
    return W_OK;
}
/* }}} */

/* {{{ static name: an XML name onto the buffer, ending in a zero byte */
static WErr name(XmlReader *x, size_t *off, const char *what, WWhy *why)
{
    if (x->i >= x->n || !name_start(x->s[x->i])) return FAIL(W_ERR_CORRUPT, "%s should begin here, with a letter, _ or :", what);
    *off = x->blen;
    size_t k = x->i;
    while (k < x->n && name_char(x->s[k])) ++k;
    if (!put(x, x->s + x->i, k - x->i) || !putc_(x, 0)) return NOMEM();
    x->i = k;                             /* no line ends in a name */
    return W_OK;
}
/* }}} */

/* {{{ static push, pop_name: the open elements */
static int push(XmlReader *x, const char *nm, int line)
{
    size_t len = strlen(nm) + 1;
    if (!grow((void **)&x->stk, &x->scap, x->slen, len, 1)) return 0;
    size_t cap = (size_t)x->ocap;
    if (!grow((void **)&x->open, &cap, (size_t)x->depth, 1, sizeof(Open))) return 0;
    x->ocap = (int)cap;
    memcpy(x->stk + x->slen, nm, len);
    x->open[x->depth++] = (Open){ x->slen, line };
    x->slen += len;
    return 1;
}
static const char *top_name(const XmlReader *x) { return x->stk + x->open[x->depth - 1].off; }
static void pop(XmlReader *x) { x->slen = x->open[--x->depth].off; if (x->depth == 0) x->root_closed = 1; }
/* }}} */

/* {{{ static start_tag: after the <, up to and past its > */
static WErr start_tag(XmlReader *x, XmlEvent *ev, WWhy *why)
{
    int line = x->line, column = (int)(x->i - x->line_start) + 1;
    step(x);                              /* the < */
    size_t noff;
    WErr e = name(x, &noff, "an element's name", why);
    if (e) return e;
    if (x->root_closed || (x->depth == 0 && x->seen_root))
        return fail_at(x, why, W_ERR_CORRUPT, line, column, "a second top element <%s> (a file holds one, and it has closed)", x->buf + noff);
    if (x->depth >= XML_MAX_DEPTH)
        return fail_at(x, why, W_ERR_CORRUPT, line, column, "<%s> is nested deeper than %d elements", x->buf + noff, XML_MAX_DEPTH);
    x->na = 0;
    int empty = 0;
    for (;;) {
        size_t before = x->i;
        skip_spaces(x);
        if (x->i >= x->n) return fail_at(x, why, W_ERR_CORRUPT, line, column, "the tag <%s> never ends (no >)", x->buf + noff);
        unsigned char c = x->s[x->i];
        if (c == '>') { step(x); break; }
        if (c == '/') {
            step(x);
            if (x->i >= x->n || x->s[x->i] != '>') return FAIL(W_ERR_CORRUPT, "a / inside the tag <%s> not followed by >", x->buf + noff);
            step(x);
            empty = 1;
            break;
        }
        if (x->i == before) return FAIL(W_ERR_CORRUPT, "the tag <%s> needs a space before its next attribute", x->buf + noff);
        size_t aname;
        if ((e = name(x, &aname, "an attribute's name", why))) return e;
        skip_spaces(x);
        if (x->i >= x->n || x->s[x->i] != '=') return FAIL(W_ERR_CORRUPT, "the attribute %s has no = and value", x->buf + aname);
        step(x);
        skip_spaces(x);
        if (x->i >= x->n || (x->s[x->i] != '"' && x->s[x->i] != '\''))
            return FAIL(W_ERR_CORRUPT, "the attribute %s's value isn't in quotes", x->buf + aname);
        unsigned char q = x->s[x->i];
        int vline = x->line, vcol = (int)(x->i - x->line_start) + 1;
        step(x);
        size_t aval = x->blen;
        for (;;) {
            if (x->i >= x->n) return fail_at(x, why, W_ERR_CORRUPT, vline, vcol, "the attribute %s's value never ends (no closing quote)", x->buf + aname);
            unsigned char v = x->s[x->i];
            if (v == q) { step(x); break; }
            if (v == '<') return FAIL(W_ERR_CORRUPT, "a < inside the attribute %s's value (written &lt;)", x->buf + aname);
            if (v == 0) return FAIL(W_ERR_CORRUPT, "a zero byte");
            if (v == '&') { if ((e = entity(x, why))) return e; continue; }
            if (v == '\r' && x->i + 1 < x->n && x->s[x->i + 1] == '\n') step(x);   /* \r\n is one line end: one space */
            if (!putc_(x, is_space(v) ? ' ' : (char)v)) return NOMEM();
            step(x);
        }
        if (!putc_(x, 0)) return NOMEM();
        for (int k = 0; k < x->na; ++k)
            if (!strcmp(x->buf + x->aoff[2 * k], x->buf + aname))
                return fail_at(x, why, W_ERR_CORRUPT, vline, vcol, "the attribute %s is written twice in <%s>", x->buf + aname, x->buf + noff);
        size_t cap = (size_t)x->acap;
        if (!grow((void **)&x->aoff, &cap, (size_t)x->na * 2, 2, sizeof(size_t))) return NOMEM();
        x->acap = (int)cap;
        x->aoff[2 * x->na] = aname;
        x->aoff[2 * x->na + 1] = aval;
        ++x->na;
    }
    if (x->na && !grow((void **)&x->attrs, &x->attrs_cap, 0, (size_t)x->na, sizeof(XmlAttr))) return NOMEM();
    for (int k = 0; k < x->na; ++k) x->attrs[k] = (XmlAttr){ x->buf + x->aoff[2 * k], x->buf + x->aoff[2 * k + 1] };
    *ev = (XmlEvent){ .kind = XML_START, .name = x->buf + noff, .attrs = x->attrs, .nattrs = x->na, .empty = empty,
                      .line = line, .column = column, .depth = x->depth };
    if (!push(x, ev->name, line)) return NOMEM();
    x->seen_root = 1;
    x->pending_end = empty;
    return W_OK;
}
/* }}} */

/* {{{ static end_tag: after the </ */
static WErr end_tag(XmlReader *x, XmlEvent *ev, WWhy *why)
{
    int line = x->line, column = (int)(x->i - x->line_start) + 1;
    step(x); step(x);                     /* the </ */
    size_t noff;
    WErr e = name(x, &noff, "the closing tag's name", why);
    if (e) return e;
    skip_spaces(x);
    if (x->i >= x->n || x->s[x->i] != '>') return FAIL(W_ERR_CORRUPT, "the closing tag </%s> never ends (no >)", x->buf + noff);
    step(x);
    if (x->depth == 0) return fail_at(x, why, W_ERR_CORRUPT, line, column, "</%s> closes nothing (no element is open)", x->buf + noff);
    if (strcmp(top_name(x), x->buf + noff))
        return fail_at(x, why, W_ERR_CORRUPT, line, column, "</%s> closes <%s>, which opened on line %d", x->buf + noff, top_name(x), x->open[x->depth - 1].line);
    pop(x);
    *ev = (XmlEvent){ .kind = XML_END, .name = x->buf + noff, .line = line, .column = column, .depth = x->depth };
    return W_OK;
}
/* }}} */

/* {{{ xml_next */
WErr xml_next(XmlReader *x, XmlEvent *ev, WWhy *why)
{
    if (x->failed) { *why = x->err; return x->err.code; }
    x->blen = 0;
    if (x->pending_end) {
        x->pending_end = 0;
        if (!put(x, top_name(x), strlen(top_name(x)) + 1)) return NOMEM();
        int line = x->open[x->depth - 1].line;
        pop(x);
        *ev = (XmlEvent){ .kind = XML_END, .name = x->buf, .empty = 1, .line = line, .depth = x->depth };
        return W_OK;
    }
    if (x->i == 0 && x->n >= 2 && ((x->s[0] == 0xFF && x->s[1] == 0xFE) || (x->s[0] == 0xFE && x->s[1] == 0xFF)))
        return FAIL(W_ERR_UNSUPPORTED, "the file is in UTF-16, which isn't read (only UTF-8)");
    for (;;) {
        if (x->i >= x->n) {
            if (x->depth > 0)
                return FAIL(W_ERR_CORRUPT, "the file ends inside <%s>, which opened on line %d", top_name(x), x->open[x->depth - 1].line);
            if (!x->seen_root) return FAIL(W_ERR_CORRUPT, "the file holds no element");
            *ev = (XmlEvent){ .kind = XML_DONE, .line = x->line };
            return W_OK;
        }
        int line = x->line, column = (int)(x->i - x->line_start) + 1;
        unsigned char c = x->s[x->i];
        if (c == '<') {
            if (at(x, "<!--")) {
                if (!skip_past(x, "-->")) return fail_at(x, why, W_ERR_CORRUPT, line, column, "a comment that never ends (no -->)");
                continue;
            }
            if (at(x, "<![CDATA[")) {
                if (x->depth == 0) return FAIL(W_ERR_CORRUPT, "a CDATA section outside the top element");
                for (int k = 0; k < 9; ++k) step(x);
                int tline = x->line;
                for (;;) {
                    if (x->i >= x->n) return fail_at(x, why, W_ERR_CORRUPT, line, column, "a CDATA section that never ends (no ]]>)");
                    if (at(x, "]]>")) { step(x); step(x); step(x); break; }
                    unsigned char v = x->s[x->i];
                    if (v == 0) return FAIL(W_ERR_CORRUPT, "a zero byte");
                    if (v == '\r' && x->i + 1 < x->n && x->s[x->i + 1] == '\n') { step(x); continue; }
                    if (!putc_(x, v == '\r' ? '\n' : (char)v)) return NOMEM();
                    step(x);
                }
                if (!putc_(x, 0)) return NOMEM();
                *ev = (XmlEvent){ .kind = XML_TEXT, .text = x->buf, .text_len = x->blen - 1, .cdata = 1,
                                  .line = tline, .column = column, .depth = x->depth };
                return W_OK;
            }
            if (at(x, "<!DOCTYPE")) {
                if (x->seen_root) return FAIL(W_ERR_CORRUPT, "a DOCTYPE after the top element began");
                size_t k = x->i;
                while (k < x->n && x->s[k] != '>' && x->s[k] != '[') ++k;
                if (k >= x->n) return FAIL(W_ERR_CORRUPT, "a DOCTYPE that never ends (no >)");
                if (x->s[k] == '[') return FAIL(W_ERR_UNSUPPORTED, "a DOCTYPE with declarations of its own, which aren't read");
                while (x->i <= k) step(x);
                continue;
            }
            if (at(x, "<!")) return FAIL(W_ERR_CORRUPT, "<! begins neither a comment, CDATA nor a DOCTYPE");
            if (at(x, "<?")) {
                if (!skip_past(x, "?>")) return fail_at(x, why, W_ERR_CORRUPT, line, column, "a <? that never ends (no ?>)");
                continue;
            }
            if (at(x, "</")) return end_tag(x, ev, why);
            return start_tag(x, ev, why);
        }
        /* text, up to the next < */
        int only_spaces = 1;
        while (x->i < x->n && x->s[x->i] != '<') {
            unsigned char v = x->s[x->i];
            if (v == 0) return FAIL(W_ERR_CORRUPT, "a zero byte");
            if (v == '&') { only_spaces = 0; WErr e = entity(x, why); if (e) return e; continue; }
            if (!is_space(v)) only_spaces = 0;
            if (v == '\r' && x->i + 1 < x->n && x->s[x->i + 1] == '\n') { step(x); continue; }
            if (!putc_(x, v == '\r' ? '\n' : (char)v)) return NOMEM();
            step(x);
        }
        if (x->depth == 0) {
            if (!only_spaces) return fail_at(x, why, W_ERR_CORRUPT, line, column, "text outside the top element");
            x->blen = 0;
            continue;
        }
        if (!putc_(x, 0)) return NOMEM();
        *ev = (XmlEvent){ .kind = XML_TEXT, .text = x->buf, .text_len = x->blen - 1, .line = line, .column = column, .depth = x->depth };
        return W_OK;
    }
}
/* }}} */

/* {{{ xml_attr */
const char *xml_attr(const XmlEvent *ev, const char *nm)
{
    for (int k = 0; k < ev->nattrs; ++k)
        if (!strcmp(ev->attrs[k].name, nm)) return ev->attrs[k].value;
    return NULL;
}
/* }}} */
