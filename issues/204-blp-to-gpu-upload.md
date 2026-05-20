# 204 — BLP → GPU Texture Upload

## Current behavior

Phase 1 issue [107](./107-blp-parser.md) gave us `blp_t` — parsed BLP
images sitting in CPU memory, optionally decompressed to RGBA8 by a
helper. There is no path from a `blp_t` to a GPU texture handle. The
Phase 2 sprite + UI + scene work all need that path.

## Intended behavior

A single function that turns a `blp_t` into a raylib `Texture2D` (which
is the GPU handle every downstream renderer wants):

```c
err_t gfx_texture_from_blp(const blp_t *blp, gfx_texture_t *out);

void  gfx_texture_unload(gfx_texture_t *);
Texture2D gfx_texture_raw(const gfx_texture_t *);  /* for raylib draws */
```

`gfx_texture_t` wraps `Texture2D` plus our bookkeeping (logical asset
name, bytes uploaded — for the HUD readout, mip count).

The upload path:

1. If the BLP is DXT-compressed **and** the driver advertises
   `GL_EXT_texture_compression_s3tc`, upload mips directly with
   `rlLoadTextureCubemap`-style raw GL via `rlgl`. raylib's high-level
   `LoadTexture` does not take pre-compressed mip chains, so this is
   one of the cases where we use `rlgl`.
2. If the BLP is DXT-compressed and the driver lacks S3TC, fall back to
   `blp_to_rgba8` and upload uncompressed. Per CLAUDE.md, fallbacks are
   warnings — log a `warn` line naming the BLP and the driver, and
   create an issue file if any production-path texture takes this
   branch.
3. If the BLP is uncompressed BGRA, swizzle to RGBA on upload (raylib
   wants `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`).
4. If the BLP is palettized, the [107](./107-blp-parser.md) decoder
   already produced RGBA — upload as that.

A small **texture cache** sits in front of all of this, keyed by
logical asset name. Asking for the same BLP twice returns the same
`gfx_texture_t`.

## Suggested implementation steps

1. Create `src/gfx/` subsystem (the umbrella for cross-cutting raylib
   helpers used by the renderer — texture, mesh, framebuffer wrappers).
2. Implement the S3TC capability check at startup (cached, queried
   through `rlglGetCurrent…` / `glGetString(GL_EXTENSIONS)`).
3. Implement the four-branch upload path.
4. Implement the cache. Eviction policy: LRU above a configurable
   memory budget (default 256 MiB for v1).
5. Tests: a synthetic DXT5 BLP round-trips through upload + readback,
   the cache returns the same handle on a second request, eviction
   actually frees GPU memory (verified through raylib's `MemFree` log
   and our own counter).

## Acceptance

- Loading `Interface/Glues/Models/UI_MainMenu/UI_MainMenu.blp` and
  drawing it as a `DrawTexture` produces a recognizable login-screen
  background.
- A second request returns the cached handle (verified by pointer
  equality + log line).
- Soft cap is honored: requesting more textures than the budget evicts
  the oldest, with a `trace` log per eviction.

## Stress markers

The CPU-decompression fallback for non-S3TC drivers is the one path we
should not need in production but is also the easiest path to develop
against. Make sure the warning is loud enough that we notice it during
day-to-day work on machines that support S3TC perfectly well — silent
fallbacks are a CLAUDE.md violation.

## Related

- [107](./107-blp-parser.md)
- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: [107](./107-blp-parser.md), [201](./201-window-and-context.md).
- Blocks: [205](./205-sprite-renderer.md), [206](./206-bitmap-text.md),
  every later texture-consuming renderer.
