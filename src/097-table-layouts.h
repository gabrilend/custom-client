/*
 * 097-table-layouts.h (issue 106: layouts; issue 1109 steps 1r, 1w) - what
 * each column of the client's tables is, for 3.3.5a (build 12340)
 *
 * For a general audience: a table (016-dbc.h) is rows of 4-byte columns
 * with nothing in the file saying what a column means. A layout says it:
 * each column's name, its kind (a whole number, a signed one, a decimal,
 * a text, or a localised text: sixteen texts, one a client language,
 * then a mask) and what a made-up row should hold there, for every table
 * this client reads and every table world-edit-to-execute's converter
 * writes for it. The tables (074) use them three ways: a copy too narrow
 * for the columns this client reads is refused; a copy of another width
 * than the layout's is said; and a placeholder row (a row no place has,
 * where a reader takes one) is filled by its layout, so its columns hold
 * sensible values (a size of 1, an opaque alpha, a visible
 * "unknown Map 900" for a name) rather than zeros.
 *
 * Where each column comes from, and how sure it is:
 *   read_by  the client's own readers read it at this place (025, 026,
 *            040, 041, 074, 087, 088): checked from code, its place and
 *            kind as those readers use it;
 *   NULL     unchecked: its name and kind are world-edit-to-execute's
 *            (src/wowbuild/dbc.lua's LAYOUTS, themselves marked
 *            unchecked there: recalled from AzerothCore's DBCfmt.h), or,
 *            for a table the converter doesn't write, recalled here; no
 *            source in either project lists them.
 * Every table's width is unchecked: no 3.3.5a file is in either project
 * to measure, so each width is the converter's (its LAYOUTS' counts),
 * the baseline set's (085), the test program's made tables, or what a
 * reader's own comment lists, as `width_from` says; each agrees with the
 * last column a reader reads. So a copy of another width is said, not
 * refused (1109 step 1r, decision 135); one narrower than the columns
 * read is refused, since a reader would read past its row.
 *
 * Agreeing with the converter (1109 step 1w): tests/100-layouts-match-
 * converter.lua has world-edit-to-execute's own dbc.lua write a row of
 * every table it writes (eleven, the light tables since its #260),
 * reads it back through 016 by these layouts, and compares each width,
 * column name, kind, value and localised mask, and that every column
 * the converter says this client reads is marked read here. That makes
 * the two copies agree with each other, not with a 3.3.5a file: the
 * widths above stay unchecked against the original, and the nine tables
 * the converter doesn't write (CreatureDisplayInfoExtra, her character's
 * six, LiquidType and LightSkybox) have no converter file to agree with.
 *
 * Window-free, no state: in the reader library (READERS, so its Lua
 * checks can ask it) and so in CORE (the test program and the executable).
 */
#ifndef EG_TABLE_LAYOUTS_H
#define EG_TABLE_LAYOUTS_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

typedef enum { TABLE_U32, TABLE_I32, TABLE_F32, TABLE_STR, TABLE_LOC } TableType;

#define TABLE_LOC_TEXTS 16            /* a localised text: 16 texts (enUS the first), then its mask */
#define TABLE_LOC_MASK  0xFF01FEu     /* the mask a 3.3.5a file carries (world-edit-to-execute's dbc.LOC_MASK: unchecked) */

/* A run of `count` columns of one kind (count 0 or 1: one, named `name`;
 * more: name1 .. nameN; a TABLE_LOC is 17 columns each). `def` is a
 * placeholder's number there; `unknown` 1: a text placeholder reads
 * "unknown <table> <row>" (106: "a visible 'unknown spell 133'"), else "". */
typedef struct {
    const char *name;
    TableType   type;
    int         count;
    const char *read_by;
    double      def;
    int         unknown;
} TableGroup;

typedef struct {
    const char       *name;          /* as 074 names it: "Map" is DBFilesClient\Map.dbc */
    uint32_t          fields;        /* its width in 4-byte columns */
    int               has_id;        /* 1: its first column is the row's number (074 looks rows up by it) */
    const char       *width_from;    /* where the width comes from (all unchecked against a 3.3.5a file) */
    const TableGroup *groups;        /* ending with a NULL name */
} TableLayout;

/* One column, from 1: its name ("TextureVariation2", "MapName_enUS",
 * "MapName_mask"), kind (a localised text's columns are TABLE_STR, its
 * mask TABLE_U32), reader, and placeholder value. */
typedef struct {
    char        name[64];
    TableType   type;
    const char *read_by;
    double      def;
    int         unknown;
} TableColumn;

/* The layout of table `name` (any capitals), or NULL when there is none. */
const TableLayout *table_layout(const char *name);
/* Every layout, in order: the i-th, or NULL past the last. */
const TableLayout *table_layout_at(int i);

/* Column `k` (from 1) of `l`: 1 with `*out`, 0 past its width. */
int table_layout_column(const TableLayout *l, uint32_t k, TableColumn *out);
/* The last column a reader of this client reads (0: none). */
uint32_t table_layout_read_to(const TableLayout *l);
/* How many of its columns a reader reads (checked from code). */
uint32_t table_layout_checked(const TableLayout *l);

/* A copy `fields` wide, from `where`: W_OK when a reader's columns all
 * fit; W_ERR_CORRUPT, naming the table, the place, the width and the
 * last column read, when it is narrower. (Another width than the
 * layout's is the caller's to say: it may be the layout that is wrong.) */
WErr table_layout_fits(const TableLayout *l, uint32_t fields, const char *where, WWhy *why);

/* A table of one row, `fields` wide (the layout's width, or a copy's
 * when the copies say otherwise), as a WDBC file's bytes (malloc'd,
 * `*len`): row `id`, each column the layout's placeholder value
 * (columns past the layout 0), its texts in its own text block. NULL
 * when out of memory, or `fields` is 0. */
unsigned char *table_layout_placeholder(const TableLayout *l, uint32_t fields, uint32_t id, size_t *len);

#endif
