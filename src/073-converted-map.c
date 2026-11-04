/*
 * 073-converted-map.c (issue 1109) - see 073-converted-map.h
 *
 * For a general audience: map.lua read as data, then each field the
 * client uses held to its type and range, each refusal naming the field
 * and its line.
 */
#include "073-converted-map.h"
#include "013-errors.h"
#include "059-loose-folders.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* {{{ static fields: one field of a table, held to its type */
static WErr want(const LuaValue *t, const char *table, const char *key, LuaDataType type, int required, const LuaValue **out, const char *where, WWhy *why)
{
    const LuaValue *v = lua_value_field(t, key);
    *out = v;
    if (!v) return required ? w_fail(why, W_ERR_REFUSED, "%s line %d: %s has no %s; the W client needs it", where, lua_value_line(t), table, key) : W_OK;
    if (lua_value_type(v) != type)
        return w_fail(why, W_ERR_REFUSED, "%s line %d: %s.%s is %s; it must be %s", where, lua_value_line(v), table, key,
                      lua_type_name(lua_value_type(v)), lua_type_name(type));
    return W_OK;
}
static WErr number(const LuaValue *t, const char *table, const char *key, int required, double *out, int *given, const char *where, WWhy *why)
{
    const LuaValue *v;
    WErr rc = want(t, table, key, LUA_DATA_NUMBER, required, &v, where, why);
    if (given) *given = v != NULL;
    if (rc || !v) return rc;
    *out = lua_value_number(v);
    if (!isfinite(*out)) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s.%s is not a finite number", where, lua_value_line(v), table, key);
    return W_OK;
}
static WErr whole(const LuaValue *t, const char *table, const char *key, int required, double lo, double hi, int *out, const char *where, WWhy *why)
{
    double d = 0;
    int given;
    WErr rc = number(t, table, key, required, &d, &given, where, why);
    if (rc || !given) return rc;
    if (d != floor(d) || d < lo || d > hi)
        return w_fail(why, W_ERR_REFUSED, "%s line %d: %s.%s is %g; it must be a whole number from %g to %g", where,
                      lua_value_line(lua_value_field(t, key)), table, key, d, lo, hi);
    *out = (int)d;
    return W_OK;
}
static WErr text(const LuaValue *t, const char *table, const char *key, int required, char *out, size_t n, const char *where, WWhy *why)
{
    const LuaValue *v;
    WErr rc = want(t, table, key, LUA_DATA_TEXT, required, &v, where, why);
    if (rc || !v) return rc;
    const char *s = lua_value_text(v);
    if (strlen(s) >= n) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s.%s is %zu bytes; at most %zu", where, lua_value_line(v), table, key, strlen(s), n - 1);
    snprintf(out, n, "%s", s);
    return W_OK;
}
/* }}} */

/* {{{ converted_map_read */
WErr converted_map_read(const char *folder, ConvertedMap *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    char path[700];
    if ((size_t)snprintf(path, sizeof path, "%s/%s", folder, CONVERTED_MAP_FILE) >= sizeof path)
        return w_fail(why, W_ERR_REFUSED, "the build folder's name is too long (%zu bytes)", strlen(folder));
    LuaData *d;
    WErr rc = lua_data_read(path, CONVERTED_MAP_MOST, &d, why);
    if (rc == W_ERR_NOT_FOUND)
        return w_fail(why, rc, "%s has no %s: not a map converted by world-edit-to-execute's W02c (which writes it)", folder, CONVERTED_MAP_FILE);
    if (rc) return rc;
    const LuaValue *top = lua_data_top(d), *format, *map, *start, *rule;
    double f = 0;
    if (lua_value_type(top) != LUA_DATA_TABLE) rc = w_fail(why, W_ERR_REFUSED, "%s returns %s; it returns a table", path, lua_type_name(lua_value_type(top)));
    if (!rc && !(rc = want(top, "map.lua", "format", LUA_DATA_NUMBER, 1, &format, path, why)) && (f = lua_value_number(format)) != 1)
        rc = w_fail(why, W_ERR_UNSUPPORTED, "%s line %d: format %g; this client reads format 1 (a newer world-edit-to-execute wrote it?)", path, lua_value_line(format), f);
    if (!rc) rc = want(top, "map.lua", "map", LUA_DATA_TABLE, 1, &map, path, why);
    if (!rc) rc = whole(map, "map", "id", 1, 1, 65535, &out->id, path, why);
    if (!rc) rc = text(map, "map", "directory", 1, out->directory, sizeof out->directory, path, why);
    if (!rc) {
        const char *s = out->directory;
        int ok = s[0] != '\0';
        for (; *s && ok; ++s) ok = (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_';
        if (!ok) rc = w_fail(why, W_ERR_REFUSED, "%s line %d: map.directory \"%s\" isn't 1 to 48 letters, digits and _ (a folder under World\\Maps, never a path)",
                             path, lua_value_line(lua_value_field(map, "directory")), out->directory);
    }
    if (!rc) rc = text(map, "map", "name", 0, out->name, sizeof out->name, path, why);
    if (!rc) rc = whole(map, "map", "area", 0, 0, 65535, &out->area, path, why);
    if (!rc) rc = want(top, "map.lua", "start", LUA_DATA_TABLE, 1, &start, path, why);
    if (!rc) rc = number(start, "start", "x", 1, &out->start_x, NULL, path, why);
    if (!rc) rc = number(start, "start", "y", 1, &out->start_y, NULL, path, why);
    if (!rc) rc = number(start, "start", "z", 0, &out->start_z, &out->has_z, path, why);
    if (!rc && (fabs(out->start_x) > 17066.67 || fabs(out->start_y) > 17066.67))
        rc = w_fail(why, W_ERR_REFUSED, "%s line %d: the start (%g, %g) is off the world (17066.67 yards from its middle at most)", path, lua_value_line(start), out->start_x, out->start_y);
    if (!rc) rc = want(top, "map.lua", "wc3_to_wow", LUA_DATA_TABLE, 1, &rule, path, why);
    if (!rc) rc = number(rule, "wc3_to_wow", "scale", 1, &out->wc3_scale, NULL, path, why);
    if (!rc && !(out->wc3_scale > 0)) rc = w_fail(why, W_ERR_REFUSED, "%s line %d: wc3_to_wow.scale is %g; yards per WC3 unit are above 0", path, lua_value_line(rule), out->wc3_scale);
    if (!rc) rc = number(rule, "wc3_to_wow", "x0", 1, &out->wc3_x0, NULL, path, why);
    if (!rc) rc = number(rule, "wc3_to_wow", "y0", 1, &out->wc3_y0, NULL, path, why);
    if (!rc) rc = text(top, "map.lua", "creatures", 0, out->creatures, sizeof out->creatures, path, why);
    if (!rc && out->creatures[0]) {
        /* inside the folder: relative, no part that climbs out */
        const char *s = out->creatures;
        int bad = s[0] == '/' || s[0] == '\\' || strchr(s, ':') != NULL;
        for (const char *p = s; !bad && *p; ) {
            const char *e = p + strcspn(p, "/\\");
            if (e - p == 2 && p[0] == '.' && p[1] == '.') bad = 1;
            p = *e ? e + 1 : e;
        }
        if (bad) rc = w_fail(why, W_ERR_REFUSED, "%s line %d: creatures \"%s\" isn't a path inside the build folder", path,
                             lua_value_line(lua_value_field(top, "creatures")), out->creatures);
    }
    lua_data_free(d);
    if (rc) memset(out, 0, sizeof *out);
    return rc;
}
/* }}} */

/* {{{ converted_map_from_wc3 */
void converted_map_from_wc3(const ConvertedMap *m, double wc3_x, double wc3_y, double *x, double *y)
{
    *x = m->wc3_x0 + m->wc3_scale * wc3_y;
    *y = m->wc3_y0 - m->wc3_scale * wc3_x;
}
/* }}} */
