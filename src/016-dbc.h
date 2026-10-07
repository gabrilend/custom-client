/*
 * 016-dbc.h (issue 173k; the C twin of lib/dbc.lua) - the game's own tables
 *
 * For a general audience: the client keeps its lists (skin colours, hair
 * styles, which texture each uses) in simple tables: a header, rows of
 * equal width, then one block of all the text the rows point into.
 * Layout: "WDBC", rows, fields a row, bytes a row, bytes of text (uint32
 * each); then the rows; then the text. Every field here is 4 bytes; the
 * caller says which are text.
 */
#ifndef EG_DBC_H
#define EG_DBC_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const unsigned char *data;    /* the whole file, owned by the caller */
    uint32_t rows, fields, row_len, text_len;
    const unsigned char *text;
} Dbc;

WErr         dbc_open(Dbc *d, const unsigned char *s, size_t len, WWhy *why);
uint32_t    dbc_u32(const Dbc *d, uint32_t row, uint32_t field);    /* field from 1, as the Lua */
const char *dbc_text(const Dbc *d, uint32_t row, uint32_t field);

#endif
