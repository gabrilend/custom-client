# 203 — Shader Pipeline

## Current behavior

raylib ships default shaders that handle most of its built-in drawing
(sprites, fonts, basic 3D models). They are fine for the very first
textured quad but won't carry us into terrain splatting, M2 skinning, or
the custom material kinds the renderer needs. There is no
project-managed way to load, compile, link, hot-reload, or hash custom
shaders.

## Intended behavior

A shader subsystem that:

- Loads GLSL source from disk (`shaders/<name>.vert` + `<name>.frag`).
- Compiles + links via raylib's `LoadShader` (which sits on top of
  `rlLoadShaderCode`).
- Pretty-prints driver error logs through our `log_error` rather than
  raylib's stdout TraceLog.
- Caches by hash of the combined sources, so the same vert/frag pair is
  only compiled once per process.
- Supports **permutations** via simple `#define KEY value` injection
  before the source — used for "M2 transparent vs. opaque," "terrain N
  layers," "lit vs. unlit" without N source files.
- Supports **hot reload** in dev builds: watching `shaders/` and
  recompiling on save, swapping the cached shader live. Catches "I
  tweaked GLSL, restart the world" pain early.

```c
typedef struct shader shader_t;

err_t shader_load(const char *name, const shader_define_t *defs, int n_defs,
                  shader_t **out);
void  shader_unload(shader_t *);

void  shader_set_mat4 (shader_t *, const char *uniform, mat4_t v);
void  shader_set_vec4 (shader_t *, const char *uniform, vec4_t v);
void  shader_set_int  (shader_t *, const char *uniform, int    v);
void  shader_set_tex  (shader_t *, const char *uniform, int slot, Texture2D);

Shader shader_raw(const shader_t *);   /* for raylib Begin/EndShaderMode  */
```

`shader_t` is a thin wrapper around raylib's `Shader` plus our caching
metadata. The escape hatch (`shader_raw`) is there because some raylib
draw calls require a raylib `Shader` directly.

## Suggested implementation steps

1. Create `src/shader/` subsystem.
2. Implement source loading + the `#define` injector (prepend after
   `#version`).
3. Implement compile + link + uniform-location caching. Locations
   resolved lazily on first set.
4. Wire compile errors through `log_error(SUBSYS_GFX, …)`, including
   the file + line from the driver log when present.
5. Implement the cache (key = SHA1 of post-injection source).
6. Implement hot-reload as an optional behavior, enabled only when
   `cfg->render.hot_reload_shaders` is true. Polling-based watcher is
   fine for v1 — no need for inotify just yet.
7. Write a "placeholder" shader pair: `placeholder.vert` and
   `placeholder.frag` that draw a magenta-tinted version of whatever
   they're applied to. Used as the fallback when a referenced shader
   fails to load (loud, visible, easy to spot — not silent).
8. Tests: load + use a known-good shader, attempt to load a known-bad
   shader and observe the error path, hot-reload swaps the cached
   pointer.

## Acceptance

- `shader_load("textured", NULL, 0, &s)` returns a usable shader.
- A deliberately broken shader logs a useful error and returns
  `ERR_GL` without crashing.
- In dev builds, editing `shaders/textured.frag` and saving causes the
  next frame to use the updated shader.

## Stress markers

Hot reload is the kind of feature that's irreplaceable once you have it
and feels frivolous when you're writing it. Build it now while the
shader surface is small.

## Related

- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: [201](./201-window-and-context.md), [202](./202-math-primitives.md).
- Blocks: [205](./205-sprite-renderer.md), [206](./206-bitmap-text.md),
  and every renderer in later phases.
