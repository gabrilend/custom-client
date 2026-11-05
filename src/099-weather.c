/*
 * 099-weather.c (issue 1109 step 1v) - the server's weather; see 099-weather.h
 *
 * For a general audience: reads the weather message's nine bytes and
 * eases the storm's strength from the old weather to the new.
 */
#include "099-weather.h"
#include "013-errors.h"

#include <math.h>
#include <string.h>

/* {{{ weather_msg_read, weather_msg_storm */
WErr weather_msg_read(const unsigned char *body, size_t len, WeatherMsg *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!body || len != WEATHER_BODY_BYTES)
        return w_fail(why, W_ERR_CORRUPT, "a weather message's body is %zu bytes; this client reads %d (state, grade, abrupt: the layout is recalled, unchecked)",
                      body ? len : 0, WEATHER_BODY_BYTES);
    out->state = body[0] | body[1] << 8 | body[2] << 16 | (uint32_t)body[3] << 24;
    uint32_t g = body[4] | body[5] << 8 | body[6] << 16 | (uint32_t)body[7] << 24;
    float grade;
    memcpy(&grade, &g, 4);
    if (!isfinite(grade)) return w_fail(why, W_ERR_CORRUPT, "a weather message's grade isn't a number (bytes 4-7: 0x%08x)", (unsigned)g);
    out->grade = grade < 0 ? 0 : grade > 1 ? 1 : grade;
    if (body[8] > 1) return w_fail(why, W_ERR_CORRUPT, "a weather message's abrupt byte is %u; it is 0 or 1", (unsigned)body[8]);
    out->abrupt = body[8];
    return W_OK;
}
float weather_msg_storm(const WeatherMsg *m) { return m->state == WEATHER_FINE ? 0 : m->grade; }
/* }}} */

/* {{{ weather_init, weather_take, weather_strength */
void weather_init(Weather *w) { memset(w, 0, sizeof *w); }

void weather_take(Weather *w, const WeatherMsg *m, double seconds)
{
    float at = weather_strength(w, seconds);
    w->now = *m;
    w->to = weather_msg_storm(m);
    w->from = m->abrupt ? w->to : at;
    w->since = seconds;
    ++w->changes;
}

float weather_strength(const Weather *w, double seconds)
{
    double f = (seconds - w->since) / WEATHER_FADE_SECONDS;
    if (f >= 1 || w->from == w->to) return w->to;
    if (f <= 0) return w->from;
    return w->from + (w->to - w->from) * (float)f;
}
/* }}} */
