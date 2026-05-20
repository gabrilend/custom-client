# 107 — BLP Parser

## Current behavior

The client cannot decode BLP textures. Every texture in the game uses
this format.

## Intended behavior

A standalone BLP2 decoder that reads the header, all mip levels, and
hands back a typed `blp_t` containing the pixel data (still in its
on-disk form for DXT variants, decompressed only when asked).

```c
err_t blp_decode    (const buf_t *in, blp_t **out);
void  blp_free      (blp_t *);

const blp_mip_t *blp_mip(const blp_t *, int level);  /* level 0 = base */
int              blp_mip_count(const blp_t *);
blp_format_t     blp_format(const blp_t *);          /* DXT1/3/5, BGRA, palette */
int              blp_width (const blp_t *);
int              blp_height(const blp_t *);
```

The decoder does *not* upload anything to a GPU — there is no GPU in
Phase 1. Phase 2 wires the BLP into a texture-upload helper.

Phase 1 demonstrates the format works by:

1. Reading `Interface/Glues/Models/UI_MainMenu/UI_MainMenu.blp` (or any
   known asset) from the MPQ chain.
2. Decoding it.
3. Writing the level-0 mip as a `.tga` to `tmp/` so you can eyeball it.

## Suggested implementation steps

1. Create `src/blp/` subsystem.
2. Parse the BLP2 header (magic, type, alpha bits, mip offsets, sizes,
   width, height).
3. Branch on format:
   - DXT1/3/5 — keep compressed, the GPU eats it natively in Phase 2.
   - Uncompressed BGRA — keep as-is.
   - Palettized — decode to BGRA using the embedded 256-entry palette.
4. Add a `blp_to_rgba8(const blp_t *, int level, buf_t *out)` helper
   that decompresses any format to RGBA8 on the CPU — for tests, dumps,
   and platforms without S3TC.
5. Write a tiny TGA writer (only used by tools/tests).
6. Tests: round-trip a synthetic BLP, decode a real Blizzard BLP and
   compare its RGBA8 form against a reference image bytewise.

## Acceptance

- Phase 1 demo writes a recognizable login-screen background TGA to
  `tmp/`.
- Decoding a known DXT5 BLP and re-decompressing to RGBA8 matches a
  reference byte-for-byte.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#blp--textures)
- Blocked by: [105](./105-mpq-reader.md).
- Blocks: [108](./108-phase-demo.md), Phase 2 texture upload.
