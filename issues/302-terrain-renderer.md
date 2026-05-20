# 302 — Terrain Renderer (Mesh Builder + Splat Shader)

## Current behavior

Phase 1 + Phase 2 + [301](./301-adt-parser.md) get us as far as a parsed
`adt_t` sitting in CPU memory. There is no path from that to vertices on
the GPU and certainly no terrain shader that knows how to splat up to
four textures together using the chunk's alpha maps.

## Intended behavior

Two pieces, intentionally separated per CLAUDE.md ("write data
generation functionality, and then separately and abstracted away,
write data viewing functionality"):

### Data generation: `terrain_mesh_build`

```c
err_t terrain_mesh_build(const adt_t *, terrain_mesh_t **out);
void  terrain_mesh_free (terrain_mesh_t *);

int   terrain_mesh_chunk_count(const terrain_mesh_t *);
const terrain_chunk_mesh_t *terrain_mesh_chunk(const terrain_mesh_t *, int);
```

For each of the 256 chunks per tile, build one indexed mesh:

- **145 vertices**: position (xyz, world-space), normal (xyz), 2 uv sets
  (the outer-9×9 quads need different UVs than the inner-8×8 splits;
  the second set comes "for free" by computing both during build).
- **Triangle strip per quad**: each of the 8×8 quads is split into four
  triangles around the inner center vertex, giving 8×8×4 = 256
  triangles per chunk, 768 indices per chunk.
- Per-chunk axis-aligned bounding box for culling.
- Per-chunk reference back to its `adt_chunk_t` for the renderer to
  read texture layers + alpha maps.

This is pure data — no GL calls. Lives in `src/terrain/mesh.c`.

### Data viewing: `terrain_renderer`

```c
err_t terrain_renderer_init   (terrain_renderer_t **out);
void  terrain_renderer_close  (terrain_renderer_t *);

err_t terrain_renderer_upload (terrain_renderer_t *,
                               const terrain_mesh_t *,
                               const adt_t *,           /* for texture refs */
                               terrain_handle_t *out);
void  terrain_renderer_unload (terrain_renderer_t *, terrain_handle_t);

void  terrain_renderer_draw   (terrain_renderer_t *, const camera3d_t *,
                               const terrain_handle_t *, int count);
```

The renderer:

- Uploads the per-chunk vertex + index buffers via `rlgl` (one big VBO
  per tile, sub-ranges per chunk to keep draw count low).
- Resolves each chunk's up-to-4 texture layer names against the BLP
  cache (issue [204](./204-blp-to-gpu-upload.md)) and the per-chunk
  alpha map blob.
- Issues one draw per chunk that's not frustum-culled. Each draw binds
  the chunk's alpha map texture and four layer textures (sampler
  slots 0–4) then draws the chunk's index range.
- The shader (`terrain.vert` + `terrain.frag`) is loaded via the
  [shader subsystem](./203-shader-pipeline.md) with a `LAYERS=n`
  permutation `#define` so chunks with fewer layers don't pay for
  unused samplers in the inner loop.

### The splat shader (sketch)

Vertex shader: model→world→view→proj, pass through normal + the two uv
sets.

Fragment shader pseudocode (LAYERS=4 permutation):

```
vec4 c1 = texture(layer1, uv * layer1_scale);
vec4 c2 = texture(layer2, uv * layer2_scale);
vec4 c3 = texture(layer3, uv * layer3_scale);
vec4 c4 = texture(layer4, uv * layer4_scale);
vec3 a  = texture(alpha_map, uv_outer).rgb;   /* r,g,b ∈ [0,1] */
vec3 base = c1.rgb;
base = mix(base, c2.rgb, a.r);
base = mix(base, c3.rgb, a.g);
base = mix(base, c4.rgb, a.b);
out_color = vec4(base * lambert_directional(normal), 1.0);
```

The alpha map is a 64×64×3 texture (one byte per RGB channel, one
channel per layer 2..4). Layer 1 is always the base.

## Suggested implementation steps

1. Create `src/terrain/` subsystem.
2. Implement `terrain_mesh_build` purely from the parsed `adt_t`.
   - Vertex positions: `(chunk_origin + sample_xy_offset, height)`.
   - Normals: decode the int8 MCNR triplets, normalize.
   - UVs: two sets — outer for alpha map sampling, inner for per-layer
     tiled sampling (the per-layer scale lives in `MCLY`).
   - Bounding box: compute min/max over the 145 verts.
3. Write the indexed-mesh generator: 8×8 quads × 4 triangles per quad.
4. Implement the renderer init: load the terrain shader permutations
   (LAYERS=1..4), create the `rlgl` VBO/IBO/VAO.
5. Implement `terrain_renderer_upload`: copy verts + indices into the
   tile's region of the persistent buffer, prepare the per-chunk alpha
   map texture (each chunk has its own 64×64 RGB).
6. Implement `terrain_renderer_draw`: frustum-cull each chunk by its
   bbox (uses [307](./307-freefly-and-culling.md)'s plane extractor),
   bind textures, issue the indexed draw.
7. Write the two shader files under `shaders/terrain.{vert,frag}`,
   four-layer variant first.
8. Tests for the mesh builder: known-input → known-output bounding
   box, known triangle count, normals all unit length. Renderer tests
   are visual — covered by [308](./308-phase-demo.md).

## Acceptance

- One tile of Northshire renders with recognizable ground textures
  (grass, dirt patches, the road, river bed).
- Switching off the alpha map (forcing `a = vec3(0)`) shows only the
  base layer, providing an obvious A/B test that the shader is doing
  the right thing.
- Per-frame stats expose: chunks drawn, chunks culled, triangles
  submitted, texture binds.

## Stress markers

The alpha map's relationship to UVs is the single thing in this issue
most likely to look slightly wrong in ways that take a long time to
diagnose. The alpha is sampled with **outer** UVs (covers the entire
chunk uniformly 0..1); the layer textures are sampled with **inner**
UVs that tile across the chunk according to per-layer scale. Mixing
those up produces "the textures look fine but the blends are scrambled"
— a confusing failure mode.

Per-frame texture binds will balloon if we don't batch. v1 lives with
five binds per chunk per frame; if the demo's draw count gets painful
we revisit (texture arrays, virtual textures, etc.) under a new issue.

## Related

- [301](./301-adt-parser.md)
- [203](./203-shader-pipeline.md), [204](./204-blp-to-gpu-upload.md)
- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: 301, 203, 204, 207.
- Blocks: [308](./308-phase-demo.md).
