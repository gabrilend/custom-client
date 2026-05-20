# 301 — ADT Parser

## Current behavior

The client can read DBC and BLP, but it has no parser for ADT — the
terrain-tile format. Without it, there's no terrain to render.

## Intended behavior

A parser that turns an ADT tile's raw bytes (and its 3.x companion
files) into a typed `adt_t` containing heightmap, normals, per-chunk
texture layers + alpha maps, liquid layers, and reference lists for
doodads (M2) and WMOs placed in the tile.

### What lives where

The 3.3.5a era splits a single tile across three files:

| File              | Holds                                                  |
|-------------------|--------------------------------------------------------|
| `WorldXX_YY.adt`  | base terrain: heightmap, normals, per-chunk shadow + liquid |
| `WorldXX_YY_tex0.adt` | per-chunk texture layer table + alpha maps         |
| `WorldXX_YY_obj0.adt` | doodad + WMO reference lists                       |

The parser opens all three by name siblings and merges them into one
`adt_t`. A missing companion is an error, not a fallback — per
CLAUDE.md.

### Chunk shape

A tile is **16×16 chunks** (MCNK). Each chunk holds:

- **MCVT** — 145 height samples: a 9×9 outer grid plus an 8×8 inner
  grid, stored interleaved (row of 9, row of 8, row of 9, …).
- **MCNR** — 145 packed normals (one byte per axis + one padding byte).
- **MCLY** — up to 4 texture layers (index into MTEX, render flags,
  alpha-map offset into MCAL, effect id).
- **MCAL** — alpha maps for layers 2–4 (layer 1 is the base).
  Compressed in 3.x — see implementation steps.
- **MCSH** — baked shadow map (64×64 bits per chunk).
- **MCRF** — list of doodad + WMO reference indices for this chunk.
- **MCLQ** — liquid (water / lava / slime / ocean) layer, optional.

The top-level chunks (outside MCNK):

- **MHDR** — offsets to the other top-levels.
- **MTEX** — texture filename string table.
- **MMDX** / **MMID** — doodad (M2) filename string table.
- **MWMO** / **MWID** — WMO filename string table.
- **MDDF** — doodad placement table (file index, position, rotation, scale).
- **MODF** — WMO placement table (file index, position, rotation, bbox).

### Public surface

```c
err_t  adt_load (mpq_chain_t *, const char *base_logical_path, adt_t **out);
void   adt_free (adt_t *);

int          adt_chunk_count(const adt_t *);            /* always 256 */
const adt_chunk_t *adt_chunk(const adt_t *, int cx, int cy);  /* 0..15 */

int          adt_texture_count(const adt_t *);
const char  *adt_texture_name(const adt_t *, int i);

int          adt_doodad_count(const adt_t *);
const adt_doodad_ref_t *adt_doodad(const adt_t *, int i);
int          adt_wmo_count(const adt_t *);
const adt_wmo_ref_t *adt_wmo(const adt_t *, int i);
```

`adt_chunk_t` exposes:

```c
typedef struct {
    float      heights[145];      /* MCVT, world-units, relative to chunk origin */
    int8_t     normals[145][3];   /* MCNR, unit normals scaled to int8 */
    int        layer_count;       /* 0..4 */
    adt_layer_t layers[4];        /* texture index + flags + alpha map ptr */
    uint8_t   *alpha_maps[4];     /* alpha_maps[0] is NULL (base layer)    */
    int        doodad_ref_count;
    int       *doodad_refs;
    int        wmo_ref_count;
    int       *wmo_refs;
    bool       has_liquid;
    adt_liquid_t liquid;          /* valid iff has_liquid                  */
    vec3_t     origin_world;      /* world-space origin of this chunk      */
} adt_chunk_t;
```

## Suggested implementation steps

1. Create `src/adt/` subsystem.
2. Implement a small chunked-format reader (`iff_iter_t` — name, size,
   pointer to body, advance). The same iterator handles MHDR and the
   nested-inside-MCNK sub-chunks.
3. Parse top-level chunks (MHDR, MTEX, MMDX, MMID, MWMO, MWID, MDDF,
   MODF). String tables become arrays of `const char *` pointing into a
   single owned `char *` buffer.
4. Parse MCNK chunks. Each MCNK header carries offsets to its
   sub-chunks relative to the MCNK body start — be careful, those are
   one of the easy-to-get-wrong details.
5. Decompress MCAL alpha maps. 3.x uses one of two formats depending on
   a flag in MHDR: **uncompressed 4096-byte** (64×64 nibbles, two
   pixels per byte) or **run-length-compressed**. The renderer wants
   the decompressed 64×64 byte form either way, expanded.
6. Open and merge the `_tex0` and `_obj0` companion files. They reuse
   the same chunk names but only carry the texture-related and
   object-related chunks respectively.
7. Compute each chunk's world-space origin from the tile's grid
   coordinates and `MAP_CHUNK_SIZE`. The ADT coordinate convention is
   the source of nearly every coord-bug in this codebase — write a
   test that asserts a known chunk's origin against a hand-computed
   value.
8. Tests: round-trip a hand-built minimal ADT, parse a real Northshire
   tile, assert chunk count, assert that at least one chunk has 4
   texture layers, assert a known doodad reference resolves to a
   known M2 path.

## Acceptance

- A real Northshire ADT parses cleanly, with all 256 chunks populated,
  no error log lines emitted.
- The decompressed alpha maps round-trip a known reference (we can
  re-encode and get bit-identical bytes back, for the uncompressed
  format at least).
- Heights at a known sample point match a third-party ADT inspector
  (`wow.tools.local` / a noggit-style viewer) within float epsilon.

## Stress markers

The MCAL compression flag lives in MHDR for the *whole tile*, but
whether a *specific* layer's alpha map uses the compressed form depends
on a per-layer MCLY flag too. Both have to agree for the runtime to
know which form a given alpha map blob is in. This is the part where
"I'll just add a fallback if the bytes look wrong" is tempting and
exactly wrong — better to error loudly and figure out which flag we
read incorrectly.

The 9-then-8 row interleave of MCVT samples is non-obvious. Diagrams
help here; the implementation should carry a brief ASCII comment
showing the layout, since the natural reader's mental model is "an
N×N grid" and that's not what's on disk.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#adt--terrain-tiles)
- Blocked by: [105](./105-mpq-reader.md).
- Blocks: [302](./302-terrain-renderer.md), [303](./303-wdt-and-streaming.md).
