/*
 * 016-dbc.c (issue 173k) - see 016-dbc.h.
 */
#include "016-dbc.h"
#include "013-errors.h"

#include <stdio.h>
#include <string.h>

/* {{{ static rd32 */
static uint32_t rd32(const unsigned char *s) { return (uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24); }
/* }}} */

/* {{{ dbc_open */
WErr dbc_open(Dbc *d, const unsigned char *s, size_t len, WWhy *why)
{
    memset(d, 0, sizeof *d);
    if (!s) { w_fail(why, W_ERR_CORRUPT, "no data (the archives don't hold this table)"); return why->code; }
    if (len < 20 || memcmp(s, "WDBC", 4) != 0) { w_fail(why, W_ERR_CORRUPT, "not a WDBC table"); return why->code; }
    d->data = s;
    d->rows = rd32(s + 4); d->fields = rd32(s + 8); d->row_len = rd32(s + 12); d->text_len = rd32(s + 16);
    d->text = s + 20 + (size_t)d->rows * d->row_len;
    return 0;
}
/* }}} */

/* {{{ dbc_u32, dbc_text */
uint32_t dbc_u32(const Dbc *d, uint32_t row, uint32_t field)
{
    return rd32(d->data + 20 + (size_t)row * d->row_len + (size_t)(field - 1) * 4);
}
const char *dbc_text(const Dbc *d, uint32_t row, uint32_t field)
{
    return (const char *)d->text + dbc_u32(d, row, field);
}
/* }}} */
