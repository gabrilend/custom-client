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
naming both widths (106's acceptance). The one exception is the
baseline set's copy (always the last): its widths are recalled, not
read from a client, so one that differs from the first copy's is left
out, said on the terminal, and the places before it answer (1109 step
1l, decision 91).

A reader that searches a table by something other than its number
(her character's tables, 025: by race, sex and choice) reads it as one
list (tables_scan): every row of the first copy in its own order, then
each later copy's rows whose numbers no earlier copy holds, in theirs.
A converted map's rows are met first and win a search; the archives'
rows it doesn't replace are still there (1109 step 1q, decision 128,
lifting decision 98's first-copy-whole limit).

A row no copy has is an error naming the table and the row, except
where a reader asks for a placeholder (tables_row_or_placeholder:
041's and 088's creature looks) in a run with a resolver: then a row
made here stands in, its number in its first field and every other
field its layout's placeholder value (097: a size of 1, an opaque
alpha, a visible "unknown Map 900" name, else 0 or ""; with no layout
0 and ""), as wide as the table's copies or, when no place has the
table, its layout; the resolver says it (a warning, counted with its
other placeholders) and strict mode refuses it (106: "a missing row is
resolved like a missing asset: a placeholder and a warning; an error
in strict mode"). It lives in memory only, until tables_free, and is
made once a row (1109 step 1q, decisions 130 and 131; step 1r,
decisions 136, 137 and 139).

Each copy is checked against its table's layout (097): one narrower
than the last column this client reads is refused (W_ERR_CORRUPT);
one of another width than the layout's is said on the standard error
and read (the widths are unchecked: 1109 step 1r, decision 135).

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
    W_ERR_CORRUPT    a copy that isn't a table (dbc_open's sentence, with its place),
                     or narrower than the columns this client reads (097)
    W_ERR_REFUSED    copies of different widths (the baseline set's aside), or a name too long
  A table that failed fails the same way each time it is asked.
- `WErr tables_row_or_placeholder(Tables *ts, const char *name, uint32_t id, TableRow *out, WWhy *why);`
  As tables_row, but a row no copy has is answered by a placeholder row
  (as above: its number, every other field its layout's value) when the
  tables have a resolver, said and counted by it (060's
  asset_row_stand_in); W_OK with `out->placeholder` 1. Strict mode, no
  resolver, a table whose first column isn't a row number (097), or no
  place with a table that has no layout (no width to make it by):
  tables_row's W_ERR_NOT_FOUND.
- `int tables_placeholders(Tables *ts);`
  How many placeholder rows were made this session, all tables.
- `type TableAt`
  The rows of table `name` for a search (above): `at[k]` is the k-th
  row's copy (dbc, row there, and which copy: 0 the first place's). The
  list is the tables' own, valid until tables_free. Errors as
  tables_row's for the table (W_ERR_NOT_FOUND when no place has it).
- `type TableScan`
- `WErr tables_scan(Tables *ts, const char *name, TableScan *out, WWhy *why);`
- `int tables_copies(Tables *ts, const char *name, uint32_t rows[TABLES_COPIES_MAX], AssetSource places[TABLES_COPIES_MAX]);`
  How many copies table `name` has, and the rows of each, first place
  first (`rows` may be NULL; up to TABLES_COPIES_MAX); -1 when it can't be
  read (tables_row says why). Reads the table if it isn't yet.
- `void tables_report(Tables *ts, FILE *to);`
  Prints each table read, its copies (place and rows), how many of the
  rows asked for each copy answered, and its placeholder rows, to `to`.
