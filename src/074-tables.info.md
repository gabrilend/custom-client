# 074-tables

*Written by `scripts/generate-info` from `074-tables.h`; don't edit, re-run it.*

074-tables.h (issue 106: tables through the resolver, per row) - the
client's tables, row by row, from every place that has them

For a general audience: the client keeps its lists (maps, creature
looks, factions) in tables (016-dbc.h), one row a thing, its number in
the first column. A map converted by world-edit-to-execute brings its
own rows for the same tables (its map's row in Map.dbc, its creatures'
looks in CreatureDisplayInfo.dbc), in a table holding only those rows.
So a table here is every copy of it at once: each place the resolver
knows that holds it, first place first (packs, the server's folders,
overlays such as a test map's folder, then the original archives), and
a row is looked up by its number in the first copy that has it. A
converted map's rows win over the archives' rows of the same number,
and every other row still comes from the archives (106: "a pack's
table rows first, then Blizzard's DBC as a borrowed fallback, per
row").

Without a resolver (every run but a test map's, for now: issue 1109's
decision 4) the archives are the only copy, so a row is the archives'
row, as each reader found it before.

A table's copies must agree on its width (fields a row): a copy of
another width was written for another client build, and reading its
columns as this one's would misread every row, so it is refused,
naming both widths (106's acceptance).

Window-free: in the test program and the executable.

## What it offers

- `typedef struct Tables Tables;`
  The tables (the handle: docs/001, "Handles"), read through `assets`
  when given, else from `chain` alone; whichever is given must outlive
  it. A table is read the first time a row of it is asked for, and kept
  until tables_free. Safe to ask from more than one thread.
- `Tables *tables_new(Resolver *assets, MpqChain *chain);`
- `void tables_free(Tables *ts);`
- `type TableRow`
  A row found: the copy of the table it is in (valid until tables_free)
  and its index there, for dbc_u32 and dbc_text; where that copy came
  from (the resolver's place, and its folder or archive).
- `WErr tables_row(Tables *ts, const char *name, uint32_t id, TableRow *out, WWhy *why);`
  The row of table `name` (as the client names it, without its folder
  and ending: "Map" is DBFilesClient\Map.dbc) whose first field is `id`.
    W_OK with `*out`
    W_ERR_NOT_FOUND  no copy has that row, or no place has the table
                     (`why` says which, naming the table and the id)
    W_ERR_CORRUPT    a copy that isn't a table (dbc_open's sentence, with its place)
    W_ERR_REFUSED    copies of different widths, or a name too long
  A table that failed fails the same way each time it is asked.
- `int tables_copies(Tables *ts, const char *name, uint32_t rows[TABLES_COPIES_MAX], AssetSource places[TABLES_COPIES_MAX]);`
  How many copies table `name` has, and the rows of each, first place
  first (`rows` may be NULL; up to TABLES_COPIES_MAX); -1 when it can't be
  read (tables_row says why). Reads the table if it isn't yet.
- `void tables_report(Tables *ts, FILE *to);`
  Prints each table read, its copies (place and rows) and how many of
  the rows asked for each copy answered, to `to`.
