# 106 — DBC Parser with Data-Described Layouts

## Current behavior

The client cannot read DBC tables. Without them, "what races exist" or "what
is map id 0 called" are unanswerable. world-edit-to-execute also needs DBC
tables (to add map rows and to look up models) and would otherwise write its
own reader with its own layouts.

## Intended behavior

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
