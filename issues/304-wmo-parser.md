# 304 — WMO Parser

## Current behavior

There's no WMO support. World Map Objects — buildings, bridges,
interiors, dungeon shells, the global-WMO maps from the
[WDT parser](./303-wdt-and-streaming.md) — none of them load.

## Intended behavior

A two-stage parser, matching how WMOs are split on disk:

| File                  | Holds                                                |
|-----------------------|------------------------------------------------------|
| `<name>.wmo`          | root: header, materials, group info, portals, doodad sets, light table |
| `<name>_NNN.wmo`      | one per group (sub-mesh): geometry, BSP, lighting, indoor/outdoor flag |

```c
err_t   wmo_load (mpq_chain_t *, const char *logical_root, wmo_t **out);
void    wmo_free (wmo_t *);

int     wmo_group_count(const wmo_t *);
const wmo_group_t *wmo_group(const wmo_t *, int);

int     wmo_material_count(const wmo_t *);
const wmo_material_t *wmo_material(const wmo_t *, int);

int     wmo_portal_count(const wmo_t *);
const wmo_portal_t *wmo_portal(const wmo_t *, int);

int     wmo_doodad_set_count(const wmo_t *);          /* doodad variants */
const wmo_doodad_set_t *wmo_doodad_set(const wmo_t *, int);
```

### Root chunks

- **MOHD** — header: counts (groups, materials, portals, lights,
  doodad refs), bounding box, ambient color.
- **MOTX** — texture filename string table.
- **MOMT** — material table (texture index, flags, blend mode, shader id).
- **MOGN** — group name string table.
- **MOGI** — per-group info: bbox, flags, name offset.
- **MOSB** — skybox name (rarely populated).
- **MOPV** — portal vertices (polygon vertices, shared pool).
- **MOPT** — portal entries (vertex range, normal).
- **MOPR** — portal references (which groups each portal connects).
- **MOLT** — light table (point + spot lights, color, attenuation).
- **MODS** — doodad sets (`set_name`, first_index, count).
- **MODN** — doodad filename string table.
- **MODD** — doodad placements (file index, position, rotation, scale).

### Group chunks (one file per group)

- **MOGP** — group header: name, descriptive name, flags (indoor /
  outdoor / unlit / cant-walk / etc.), bbox, fog count, material refs.
- **MOPY** — material per triangle (which MOMT entry, flags).
- **MOVI** — vertex indices.
- **MOVT** — vertices.
- **MONR** — normals.
- **MOTV** — texture coordinates (up to 2 sets).
- **MOBA** — render batches (which material, vertex/index ranges).
- **MOLR** — light refs (which MOLT lights apply here).
- **MODR** — doodad refs (which MODD doodads sit in this group).
- **MOBN** / **MOBR** — BSP tree (used for collision; renderer will
  use the bbox + portal walk instead).
- **MOCV** — vertex colors (interior lighting baked in).
- **MLIQ** — liquid (water inside a room).

### Public surface (group)

```c
typedef struct {
    int            material;
    uint32_t       first_index;
    uint32_t       index_count;
    uint32_t       min_vertex;
    uint32_t       max_vertex;
} wmo_batch_t;

typedef struct {
    char           name[64];
    int            flags;
    aabb_t         bbox;
    int            vertex_count;
    vec3_t        *vertices;
    vec3_t        *normals;
    vec2_t        *uv0;
    vec2_t        *uv1;        /* may be NULL */
    uint32_t      *vertex_colors;  /* RGBA8, may be NULL */
    int            index_count;
    uint16_t      *indices;
    int            batch_count;
    wmo_batch_t   *batches;
    int            doodad_ref_count;
    int           *doodad_refs;
    /* portals listed via the root's MOPR -- not duplicated here */
} wmo_group_t;
```

## Suggested implementation steps

1. Create `src/wmo/` subsystem.
2. Reuse the chunked-format iterator from
   [301](./301-adt-parser.md) (same `iff_iter_t`, same shape).
3. Parse the root, hand back the group count.
4. For each group (`<root>_<NNN>.wmo`), open the sibling file, parse
   its chunks, attach to the parent `wmo_t`.
5. Index the doodad refs / portal refs / light refs through their
   shared root tables — the group's local arrays are just integer
   indices into the root's MODD / MOPT / MOLT.
6. Compute or copy each group's AABB into a normalized `aabb_t`.
7. Resolve material texture names: walk MOMT → MOTX, expose them as
   `const char *texture` on each material struct.
8. Tests against a fixture WMO (small, hand-built) and a real Blizzard
   one (Stormwind cathedral or Northshire chapel — Northshire makes
   the integration test cheaper).

## Acceptance

- A real Northshire chapel WMO parses cleanly. Group count, vertex
  count, and triangle count match an external WMO inspector.
- Material indices resolve to existing textures in the MPQ chain (a
  validation pass walks every material and confirms `mpq_chain_has`).
- All portal references resolve to valid (group, portal) pairs.

## Stress markers

The flag bits in MOGP are dense and matter at render time (indoor vs.
outdoor especially — indoor groups don't receive the world directional
light, they're lit by MOCV vertex colors instead). Capture the flag
meanings as named constants with comments, not raw hex.

The portal references in MOPR are pairs of (group, portal) — *not*
(group, group). The portal itself is the polygon that connects two
groups. Easy to read this wrong on first pass.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#wmo--world-map-objects-buildings-dungeons)
- Blocked by: [105](./105-mpq-reader.md), [301](./301-adt-parser.md) (for
  the shared chunked-format iterator).
- Blocks: [305](./305-wmo-renderer.md).
