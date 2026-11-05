/*
 * 098-wdt.c (issue 1109 step 1s) - a map's tile list; see 098-wdt.h
 *
 * For a general audience: walks the WDT file's blocks once and keeps
 * which tiles have ground, the map's flags and, for a map that is one
 * building, that building and where it stands.
 */
#include "098-wdt.h"
#include "013-errors.h"

#include <stdio.h>
#include <string.h>

static uint32_t u32(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }
/* a block's tag reads as text reversed: "MAIN" is stored "NIAM" */
static int is_tag(const unsigned char *s, size_t at, const char *tag)
{
    return s[at] == tag[3] && s[at + 1] == tag[2] && s[at + 2] == tag[1] && s[at + 3] == tag[0];
}

/* {{{ wdt_read */
WErr wdt_read(const unsigned char *s, size_t len, Wdt *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!s || len < 12 || !is_tag(s, 0, "MVER")) return w_fail(why, W_ERR_CORRUPT, "not a WDT file: it doesn't start with an MVER block (%zu bytes)", len);
    int mphd = 0, main_ = 0;
    for (size_t at = 0; at + 8 <= len;) {
        size_t size = u32(s, at + 4), body = at + 8;
        if (size > len - body) {
            char tag[5] = { (char)s[at + 3], (char)s[at + 2], (char)s[at + 1], (char)s[at], 0 };
            for (int k = 0; k < 4; ++k) if (tag[k] < 32 || tag[k] > 126) tag[k] = '?';
            return w_fail(why, W_ERR_CORRUPT, "the WDT's %s block at byte %zu says %zu bytes, past the file's end (%zu)", tag, at, size, len);
        }
        if (is_tag(s, at, "MVER") && size >= 4) out->version = u32(s, body);
        else if (is_tag(s, at, "MPHD") && size >= 4) { out->flags = u32(s, body); mphd = 1; }
        else if (is_tag(s, at, "MAIN")) {
            if (size < (size_t)WDT_TILES * 8) return w_fail(why, W_ERR_CORRUPT, "the WDT's MAIN block is %zu bytes; 64 x 64 tiles of 8 bytes are %d", size, WDT_TILES * 8);
            for (int k = 0; k < WDT_TILES; ++k) {
                out->tile_flags[k] = u32(s, body + (size_t)k * 8);
                if (out->tile_flags[k] & WDT_MAIN_HAS_ADT) ++out->ntiles;
            }
            main_ = 1;
        } else if (is_tag(s, at, "MWMO") && size)
            snprintf(out->wmo, sizeof out->wmo, "%.*s", (int)strnlen((const char *)s + body, size), s + body);
        else if (is_tag(s, at, "MODF") && size) {
            if (size < 64) return w_fail(why, W_ERR_CORRUPT, "the WDT's MODF block is %zu bytes; a placement is 64", size);
            adt_place_read(s + body, 0, &out->place);
            out->has_place = 1;
        }
        at = body + size;
    }
    if (!mphd) return w_fail(why, W_ERR_CORRUPT, "the map's WDT file has no MPHD block (read %zu bytes)", len);
    if (!main_) return w_fail(why, W_ERR_CORRUPT, "the map's WDT file has no MAIN block, so no tile list (read %zu bytes)", len);
    return W_OK;
}
/* }}} */

/* {{{ wdt_has_tile, wdt_big_alpha, wdt_wmo_only */
int wdt_has_tile(const Wdt *w, int tx, int ty)
{
    if (!w || tx < 0 || tx > 63 || ty < 0 || ty > 63) return 0;
    return (w->tile_flags[tx * 64 + ty] & WDT_MAIN_HAS_ADT) != 0;
}
int wdt_big_alpha(const Wdt *w) { return w && (w->flags & WDT_MPHD_BIG_ALPHA) ? 1 : 0; }
int wdt_wmo_only(const Wdt *w) { return w && ((w->flags & WDT_MPHD_WMO_ONLY) || (w->wmo[0] && !w->ntiles)) ? 1 : 0; }
/* }}} */
