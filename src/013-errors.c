/*
 * 013-errors.c - what went wrong; see 013-errors.h
 *
 * For a general audience: fills in an error's code and sentence, and names
 * a code for reports.
 */
#include "013-errors.h"

#include <stdarg.h>
#include <stdio.h>

/* {{{ w_fail */
WErr w_fail(WWhy *why, WErr code, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(why->text, sizeof why->text, format, ap);
    va_end(ap);
    why->code = code;
    return code;
}
/* }}} */

/* {{{ w_err_name: the names, by code (a table, as the codes are its index) */
static const char *const NAMES[W_ERR_COUNT] = {
    [W_OK]              = "W_OK",
    [W_ERR_NOT_FOUND]   = "W_ERR_NOT_FOUND",
    [W_ERR_CORRUPT]     = "W_ERR_CORRUPT",
    [W_ERR_UNSUPPORTED] = "W_ERR_UNSUPPORTED",
    [W_ERR_REFUSED]     = "W_ERR_REFUSED",
    [W_ERR_FULL]        = "W_ERR_FULL",
    [W_ERR_IO]          = "W_ERR_IO",
    [W_ERR_GPU]         = "W_ERR_GPU",
};
const char *w_err_name(WErr code)
{
    return (code >= 0 && code < W_ERR_COUNT) ? NAMES[code] : "W_ERR_(not a code)";
}
/* }}} */
