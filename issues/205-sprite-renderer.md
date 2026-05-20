# 205 — Sprite Renderer

## Current behavior

raylib's `DrawTexture` family draws one textured quad per call,
immediate-mode. That works for "first textured quad on screen" but
collapses when the UI (eventually thousands of icons per frame) drives
it. We need batching.

## Intended behavior

A small batched sprite renderer:

```c
typedef struct sprites sprites_t;

err_t sprites_init   (sprites_t **out);
void  sprites_close  (sprites_t *);

void  sprites_begin  (sprites_t *, mat4_t view_proj);
void  sprites_draw   (sprites_t *, const gfx_texture_t *,
                      rect_t src, rect_t dst, vec4_t tint, float rot);
void  sprites_end    (sprites_t *);          /* flushes pending batches */
```

Implementation strategy:

- One growing vertex buffer (pos + uv + color, 4 verts per sprite, 6
  indices per sprite, GL_TRIANGLES, dynamic).
- Batch break on texture change. Within a batch, all sprites share one
  texture and one shader.
- The default shader is the "textured" pair from
  [203](./203-shader-pipeline.md); callers can override per
  `sprites_begin` for tinted / masked / glow-style draws later.
- Built on `rlgl` rather than raylib's `DrawTexturePro` to avoid
  per-call state thrash. raylib's own batcher is fine for the simple
  case; we replace it for a known reason (we need explicit batch
  boundaries for the UI pass, and we need typed handles instead of raw
  `Texture2D`).

## Suggested implementation steps

1. Add `sprite.c` to `src/gfx/`.
2. Allocate the vertex + index buffers (start at 4 KiB sprites = 16384
   verts; grow on overflow).
3. Implement `sprites_draw`: compute the four corners in clip space
   from `dst` + `rot`, write to the CPU-side vertex array, write the
   six indices.
4. Implement the batch flush: `rlEnableShader`, set uniforms (view-proj
   matrix, sampler slot), bind the batch's texture, upload the vertex
   subrange, `rlDrawVertexArrayElements`.
5. Add the per-frame stats sink: sprites this frame, batches this
   frame, batch-break causes. Surfaced on the HUD.
6. Tests: build a known scene (3 sprites, 2 textures), assert that
   `sprites_end` issues exactly 2 batches.

## Acceptance

- Drawing 1,000 sprites of the same texture in one frame issues exactly
  one draw call.
- Alternating textures every sprite issues 1,000 batches and logs a
  `trace` line noting the high batch-break count.
- Visually matches a raylib `DrawTexturePro` reference image for the
  same coordinates within rounding error.

## Stress markers

There's a real temptation to immediately reach for a more elaborate
batcher (texture arrays, persistent mapped buffers, bindless). v1 stays
simple — the UI pass for FrameXML is well below the threshold where any
of that pays off, and we don't yet know the real bottleneck.

## Related

- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: [203](./203-shader-pipeline.md), [204](./204-blp-to-gpu-upload.md),
  [202](./202-math-primitives.md).
- Blocks: [206](./206-bitmap-text.md), [208](./208-phase-demo.md), the
  Phase 6 login UI, the Phase 7 FrameXML UI pass.
