# 106 — DBC Parser

## Current behavior

The client cannot read DBC tables. Without these, even basic things like
"what races exist" or "what's the name of map id 0" are unanswerable.

## Intended behavior

A generic DBC reader plus a handful of typed schemas for the tables
Phase 1 needs to demonstrate the format.

```c
err_t dbc_open  (mpq_chain_t *, const char *logical, dbc_t **out);
void  dbc_close (dbc_t *);

uint32_t   dbc_row_count(const dbc_t *);
const void *dbc_row     (const dbc_t *, uint32_t index);
const char *dbc_string  (const dbc_t *, uint32_t string_block_offset);
```

Per-table typed wrappers, written by hand, one struct per DBC:

```c
typedef struct {
    uint32_t id;
    uint32_t flags;
    uint32_t faction_id;
    uint32_t name_string_offset;
    /* ... */
} dbc_chr_races_row_t;
```

The Phase 1 demo only needs `Map.dbc` and `ChrRaces.dbc`. The rest land
in later phases as they're needed — there's no value in hand-writing all
~250 schemas up front.

## Suggested implementation steps

1. Create `src/dbc/` subsystem.
2. Implement the generic reader: parse `WDBC` header, validate magic,
   build row + string block pointers. The records and string block are
   kept as a single owning byte buffer to avoid per-row allocations.
3. Add `dbc_typed.h` with the small set of schemas Phase 1 needs.
4. Add a "validate that record size matches sizeof(struct)" check at
   open time per schema. Mismatch is a hard error — this catches schema
   drift between us and Blizzard's actual file.
5. Tests using a hand-built fixture DBC.

## Acceptance

- Opening `DBFilesClient/Map.dbc` from a real chain and iterating prints
  60 maps for a vanilla 3.3.5a chain.
- A schema-size mismatch is rejected at open time with a useful message.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#dbc--client-side-data-tables)
- Blocked by: [105](./105-mpq-reader.md).
- Blocks: [108](./108-phase-demo.md).
