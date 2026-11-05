# 106 — DBC Parser with Data-Described Layouts

*Filled in part by the code from wow-chat-2026 (its 173d), 2026-10-07.*

## Current behavior

Built, simpler than planned: `src/016-dbc.c` opens a WDBC table and
reads fields by number (`dbc_u32`, `dbc_text`); each reader names the
fields it uses in its own comments (Map, Light*, LiquidType,
CharSections, ItemDisplayInfo, CreatureDisplayInfo ...). In the reader
library.

**Per row, through the resolver: built 2026-11-04** (issue 1109's step
1b, `src/074-tables.c`, window-free, in the executable and the test
program). A table is every copy of it at once: each place the resolver
knows that holds it (packs, the server's folders, overlays such as a
test map's build folder, then the archives), first place first, and a
row is looked up by its number in the first copy that has it. So a map
converted by world-edit-to-execute (its W02c writes `DBFilesClient/`
tables holding only the map's own rows) brings its Map row and its
creatures' looks, and every other row still comes from the archives.
The resolver (060) has a `table` kind for it, with no placeholder, and
`asset_resolve_all` gives every place's copy. Read so: Map (043) and
CreatureDisplayInfo, CreatureDisplayInfoExtra, CreatureModelData
(041). With no resolver, which is every run but a test map's for now
(1109's decision 4), the archives are the only copy, as each reader
found them before. Copies of different widths are refused, naming the
table, both places and both widths: a check against each other, since
there are no layout files yet to check against (acceptance line 2, in
that form). A row no copy has is still an error naming the table and
the row, not the placeholder and warning asked for above: a placeholder
row needs the layouts, to know what to fill in (1109, decision 13).
Checked in `tests/044-test-client.c` ("a converted map's folder") on a
made-up archive and folder, and on the converter's real output (all
seven of its tables as wide as 3.3.5a's).

**A search across copies, and placeholder rows: built 2026-11-05**
(1109's step 1q, decisions 128 to 133). A reader that searches a table
by something other than its number (025, her character's tables) reads
it through 074 as one list: the first place's rows, then each later
place's rows whose numbers no earlier place has. A row no copy has may
be answered by a placeholder row where the reader asks for one
(`tables_row_or_placeholder`; 088's creature display and model rows,
whose look the stand-in figure then takes): its number, every other
field 0, as wide as the table's first copy, made once and kept in
memory, a warning and a count in the resolver, an error in strict mode.
Short-lived: without layouts nothing says what each field of a real
placeholder should hold, so other readers still get the error naming
the table and row.

**Layouts: built 2026-11-05** (1109's step 1r, decisions 134 to 139,
`src/097-table-layouts.c`, window-free). For twenty tables, each
column's name, kind (`u32`, `i32`, `f32`, `str`, `loc` as 16 texts and
a mask), placeholder value, and the readers that read it: the seven
world-edit-to-execute's W02c writes (Map, AreaTable, LoadingScreens,
CreatureDisplayInfo, CreatureModelData, FactionTemplate, Faction: its
`dbc.lua` LAYOUTS, all 221 columns checked equal to it), and the
thirteen this client reads (CreatureDisplayInfoExtra, her character's
six, LiquidType and the five light tables). A column a reader reads is
checked from that reader's code; the rest, and every width, are
unchecked, since neither project has a 3.3.5a file to measure. So 074
refuses a copy narrower than the last column read (naming the table,
its place, its width and that column) and only says a copy of another
width than the layout's, rather than the hard error asked for below
(decision 135). Placeholder rows take the layout's values (a size of
1, an opaque alpha, a visible "unknown Map 900" name, the rest 0 or
""), a table no place has can take one at its layout's width, and
041's window asks for its creature rows as 088 does. 026 checks the
light tables' widths by their layouts; 085's set takes its widths from
them. Checked in `tests/044-test-client.c` ("the tables' layouts").

Not built: layouts as data files and the generator of C and Lua
layouts from them (097 is C; the converter keeps its own Lua copy; the
two were checked equal by hand once, and since 1109's step 1w
`tests/100-layouts-match-converter.lua` checks it each run against an
exported world-edit-to-execute tree: the converter's eleven tables,
the light tables of its #260 among them, written by its own code and
read back by 097, agree in width, names, kinds and values; the nine
tables it doesn't write keep unchecked widths); ChrRaces, AnimationData, ItemSet and
GameObjectDisplayInfo (no reader or converter needs them yet); typed
row structs; a missing row as a placeholder for every table (only 041
and 088's creature rows take one); the light tables (Light*) through
074 (026 still opens the first place's copy whole); WC3 map mode's tables
built from map files (world-edit-to-execute's W02c builds the first:
Map, AreaTable, LoadingScreens, CreatureDisplayInfo, CreatureModelData,
Faction, FactionTemplate).

## Intended behavior

**Data tables are assets too (decided 2026-09-23).** The client must not need
Blizzard's tables to run, because we can't use them forever. It does need
*a* table for every id the server sends: the protocol talks in numbers
("cast spell 133 with display 4527 on map 571"), and the client must know what
each number means (name, icon, cast time, model). So:

- Tables are looked up through the asset resolver
  ([105a](./105a-loose-overlays-and-asset-resolver.md)) like models: a pack's
  table rows first, then Blizzard's DBC as a borrowed fallback, **per row**,
  counted like any other borrowed asset.
- A pack table uses the same layout file as the Blizzard table, so ids and
  fields line up; it may be a DBC file or a plain Lua table file.
- **For WC3 map mode the rows are generated from the map files, dynamically,
  and nothing else** (owner, 2026-09-23): "Custom maps almost always use
  custom abilities - they very rarely use the stock wc3 abilities and such.
  Usually it's a copy and modify, so the changes are recorded in the map file.
  That is what we will dynamically create these tables to be." When a map
  loads, world-edit-to-execute reads its object data (units, abilities,
  items, buffs, upgrades) and writes the rows this client and the server need,
  keyed by the ids the server will send. No WoW Blizzard rows are consulted in
  WC3 map mode; a missing row is a placeholder and a warning.
  One gap, recorded in world-edit-to-execute W02h: a copied-and-modified object
  stores only its *changed* fields plus the id of the stock object it was
  copied from. Its unchanged fields come from WC3's own stock tables.
- Nothing in the client may refuse to start because a Blizzard table is
  missing. A missing row is resolved like a missing asset: a placeholder
  (a visible "unknown spell 133") and a warning; an error in strict mode.

A generic DBC reader in the shared library, plus **layout files**: one
description per table, kept as data, from which a generator produces both the
C structs used here and the Lua layout tables used by world-edit-to-execute.
One description, two outputs, so the projects cannot disagree about a table.

```c
err_t dbc_open  (mpq_chain_t *, const char *logical, const dbc_layout_t *, dbc_t **out);
void  dbc_close (dbc_t *);

uint32_t    dbc_row_count(const dbc_t *);
const void *dbc_row      (const dbc_t *, uint32_t index);
const void *dbc_row_by_id(const dbc_t *, uint32_t id);
const char *dbc_string   (const dbc_t *, uint32_t string_block_offset);
```

A layout file (`layouts/Map.layout`) lists, in order: field name, type
(`u32`, `i32`, `f32`, `str` = uint32 offset into the string block, `loc` =
17 × uint32 localised string offsets + 1 × uint32 flags) and an optional note.
The generator writes `dbc_generated.h` (typed structs, e.g.
`dbc_map_row_t`) and `dbc_layouts.lua` for world-edit-to-execute.

At open, the reader checks `record size` from the file header against the
layout's summed field sizes. A mismatch is a hard error: it means the layout
is for a different client build.

First tables:

| Table | Needed by |
|-------|-----------|
| Map, ChrRaces | Phase 1 demo |
| AreaTable, LoadingScreens | world-edit-to-execute map registration (W02c) |
| CreatureDisplayInfo, CreatureModelData, AnimationData | Phase 4 models; world-edit-to-execute W03, W05b |
| ItemDisplayInfo, ItemSet, GameObjectDisplayInfo | world-edit-to-execute W06 (restyle catalogue) |

## Suggested implementation steps

1. Create `src/dbc/` subsystem (shared library + executable).
2. Generic reader: parse `WDBC` header, validate magic, keep records + string
   block as one owning buffer; build an id → row index on open.
3. Layout file format and the generator (a Lua script under `scripts/`, run by
   the build; its output is not hand-edited).
4. The ten layouts above.
5. Tests: hand-built fixture DBC; a deliberately wrong layout rejected at
   open; generated C struct sizes equal layout sums (compile-time assert).

## Acceptance

- `Map.dbc` from a real chain iterates and prints its maps (the count is
  printed by the demo rather than written here, since it depends on the patch level).
- A layout/record-size mismatch is rejected with the table name and both sizes.
- world-edit-to-execute loads `dbc_layouts.lua` and reads the same rows through the FFI.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#dbc--client-side-data-tables)
- Blocked by: [105](./105-mpq-reader.md).
- Blocks: [108](./108-phase-demo.md).
- [1109](./1109-test-map-from-the-editor.md) step 1b: tables per row
  (`src/074-tables.info.md`).
- **Blocks:** [810](./810-spell-description-text.md), [1003](./1003-sounds-in-the-world.md).
