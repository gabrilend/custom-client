# 105a — Loose Overlays and the Asset Resolver

## Current behavior

Nothing decides *where* an asset comes from. Without a single deciding layer,
every renderer would open the MPQ chain itself, and there would be no way to
put a replacement in front of a Blizzard file, load a converted WC3 map from a
folder, or count how much of what's on screen is still borrowed.

## Intended behavior

One function every consumer calls:

```c
typedef enum { SRC_PACK, SRC_OVERLAY, SRC_BLIZZARD, SRC_PLACEHOLDER } asset_src_t;

err_t asset_resolve(resolver_t *, const char *logical, asset_kind_t kind,
                    buf_t *out, asset_src_t *src);
void  asset_counts (const resolver_t *, asset_counts_t *out);  /* per source, per kind */
```

Lookup order:

1. **Replacement packs** (`[paths] packs`, highest priority first): loose
   folders with a manifest listing each file's provenance, similarity score and
   seal ([docs/012](../docs/012-asset-replacement-and-provenance.md)).
2. **Overlays** (`[paths] overlays`): loose folders such as a converted WC3
   map's terrain and placements from world-edit-to-execute.
3. **The MPQ chain** ([105](./105-mpq-reader.md)).
4. **Placeholder**, chosen by kind: magenta checker texture, wireframe box
   model, short beep. A warning each time (named, counted); an error when
   `[assets] strict = true`.

`asset_counts_t` holds, per asset kind (model, texture, animation, particle,
sound, font, interface), a count per source (uint32) and, for packs, how many
are past the distinctness threshold (uint32). Every phase demo prints it.

The resolver lives in the executable, not in `libwreaders.so`:
world-edit-to-execute has its own resolver (its issue W03a) because its
lookup rules for WC3 unit types differ, but both read the same pack manifest
format.

## Suggested implementation steps

1. Pack manifest format (plain text or Lua table; one entry per file:
   logical path, file, provenance id, latest score, seal flag).
2. Index packs and overlays at startup with `fs_index_folder` ([104](./104-fs-abstraction.md)).
3. `asset_resolve` walking the four steps; log a `trace` line per resolution
   naming the winner.
4. Placeholder assets built into the binary (no files needed).
5. Counters, and a `resolver_report` that prints the table.
6. Tests: a fixture pack overriding a fixture chain file; an overlay file
   beating the chain; a missing file producing a placeholder and a warning; strict mode producing an error.

## Acceptance

- A pack file replaces the Blizzard file of the same logical path.
- The counts table is printed by the Phase 1 demo.
- Strict mode turns any placeholder into a non-zero exit.

## Related

- [docs/012-asset-replacement-and-provenance.md](../docs/012-asset-replacement-and-provenance.md)
- Blocked by: [104](./104-fs-abstraction.md), [105](./105-mpq-reader.md).
- Blocks: [108](./108-phase-demo.md), [204](./204-blp-to-gpu-upload.md), every consumer of assets.
