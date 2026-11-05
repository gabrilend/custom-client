# 302 — Terrain Renderer (Mesh Builder + Splat Shader)

*Filled in part by the code from wow-chat-2026 (its 173f, 173u), 2026-10-07.*

## Current behavior

Built: preparing a tile without the window (`src/033-ground-prep.c`:
points, light from the server's slopes, the blend picture, new pictures
unpacked with their smaller copies), then sending it
(`src/038-ground.c`, a handle): each tile four meshes; every ground
picture a layer of one shared texture array; each patch's four layer
numbers on its points; the tile's blend maps one 1024 x 1024 picture;
quarters out of sight not drawn; blob shadows under characters. 0.04 ms
a frame at Astranaar.

Measured against the clean-room checklist's note 4.5
([docs/057](../docs/057-what-the-original-program-decides.md), filled
2026-10-16):
- **Already the original's:** reading the layers (big alpha by the map
  header's flag 4, 4-bit maps low half first, run-length packing by
  layer flag 512, the copied last row and column in unfixed 4-bit maps),
  the shadow bits, and one combined picture per chunk with layers 2 to 4
  in red, green and blue and the shadow in alpha (255 lit, 0 shadowed).
- **Not yet the original's:**
  - **the shadow:** darkened here to a fixed 60%. The original blends
    shadowed cells towards a colour worked out each frame from the light:
    (ambient + 3) × 85 ÷ 256 for red, green and blue (about a third of
    the ambient), by the red part of the light's shadow-opacity band
    ([306](./306-light-and-sky.md));
  - **the mix of layers** with terrain shaders is in the original's
    shipped shader files, data to read rather than guess; this client
    lays each layer over the ones below by its strength, which is the
    original's no-shader path for maps without big alpha (with big alpha
    that path **adds** each layer instead, fogged towards black);
  - **layer flags:** 128 unlit; 64 scrolling in one of eight directions
    (the flag's lowest three bits) at a slowness from 64, 48, 32, 16, 8,
    4, 2, 1 (bits 4 to 6); not handled here;
  - **shine:** a chunk marked for specular gives layer 1 a reflection
    look in the no-shader path.
- **WC3's ground** (2026-11-04): read and made ready without the window,
  [302a](./302a-wc3-ground-read-and-prepared.md) (`src/063-w3e.c`): each
  point's heights and water, each cell's tileset pictures and subtiles,
  cliff and blighted cells marked, a stroke's area redone alone; checked
  against world-edit-to-execute's Lua. Not yet drawn: painting it onto
  the one mesh beside WoW's layers, and seating cliffs on it (docs/009
  answer 1), are still this issue's.
- **One quirk**, with a correction switch off by default: a scrolling
  layer's counter wraps from 64 to 0 only when it grows, so one moving
  towards negative numbers never wraps and loses precision over a very
  long session. The correction wraps both ways.

Since 1109's step 1c (2026-11-05) the pictures a ground tile names
ask the resolver (105a: `ground_prep_pixels` through `asset_get`): in
a test-map run the map's own when its build folder has them, the
checker when no place has them; with no resolver the archives, as
before. A converted map's tiles name the game's `Tileset\...`
pictures (W02b), so its ground draws as any ADT. Since 1109's step 1d
(2026-11-05) a converted map whose build carries `war3map.w3e`, in a
run with a Warcraft III install (`assetWc3`), draws WC3's own tileset
pictures on this mesh instead (`src/078-wc3-ground.c`, through 033):
no new shader, each chunk's four layers and blend picture laid the way
WC3 lays its cells (1109 decisions 32-41). Since 1109's step 1e
(2026-11-05) its cliffs stand as WC3's own cliff models
(`src/079-wc3-cliffs.c`, placed among the tile's doodads so 042 draws
them, each cliff cell laid with its cliff tileset's ground: decisions
42-50), seated whole on their cell rather than bent to the mesh point
by point. Since step 1f its ramps stand as WC3's ramp models on their
own ground, and blighted ground wears the main tileset's blight
picture from the editor's data, blended on its blighted corners
(decisions 51-56). Since step 1g the ground's shader lays a converted
map's WC3 cells one by one (`src/080-ground-shader.c`, read by 038): a
small picture a tile names each cell's pieces (a tileset picture's
layer in the array and the subtile in it), so a chunk shows every
tileset its cells lay, each cell's own variation, and each piece's
own colours, as WC3 lays them (decisions 57-63). Since step 1h each
cliff and ramp is bent to the ground under its points, as WC3 bends
them: the ground's heights round its cell ride in its model's name and
076 raises each point by them when it reads the model, so a bent cliff
is a model of its own and a flat one still shared (decisions 64-65; a
shader reading the ground was passed over: 042 draws one mesh for
every copy). WC3's own water is read (`src/081-wc3-water.c`: its
frames and its colour by depth) but not drawn yet: 1109's decision 67
is the plan.

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
9. The original's shadow, layer mix, flags and shine (docs/057 note 4.5,
   above; link back when done): the shadow colour from the light each
   frame; the layer mix read from the original's shipped terrain
   shaders; unlit and scrolling layers with the wrap quirk and its
   switch. Tests: with an ambient of (96, 96, 96) the shadow colour is
   (32, 32, 32); with a shadow opacity of 0 shadowed ground looks lit; a
   scrolling layer with slowness 1 moves a full direction step each
   second; the counter of one scrolling negative passes −64 without
   wrapping, and wraps with the correction on.

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
