# 003 — Asset Formats

Everything the client renders or plays comes from the 3.3.5a MPQ archives.
This doc inventories the formats and where each is consumed. Each format
gets its own parser module with a corresponding `.info.md` file once it lands
in `src/`.

## Container: MPQ

- Hash + block tables. 3.3.5a archives use the 44-byte header (format
  version field = 1), which adds a high block table for offsets past 4 GB.
  (HET/BET tables belong to later formats, Cataclysm onward; not needed here.)
- Patch chain semantics matter: later archives override files in earlier
  ones by hash. Load order (3.3.5a): `common.MPQ`, `common-2.MPQ`,
  `expansion.MPQ`, `lichking.MPQ`, locale `*.MPQ`, then `patch.MPQ`,
  `patch-2.MPQ`, `patch-3.MPQ`, locale patches, then any custom patch archives
  named in config.
- **Loose overlays** sit above the whole chain: folders of plain files
  (converted WC3 maps, replacement packs) that win over any archive. This is
  how the W client loads world-edit-to-execute's converted maps; no patch
  archive is needed for them.
- The chain can answer **which** archive or overlay supplied a file.
- Implementation: **our own reader** (owner, 2026-10-07: "We use our own
  extracter"), written from the format's public description and built in
  wow-chat-2026's custom client: the hash and block tables, the
  encryption, zlib-compressed and stored sectors, the chain with its
  locale and patch archives, a memory of files already unpacked shared by
  both threads under a lock. Writing archives (patch archives for the
  stock client, the "default client compatible" seal, packing a player's
  folder of files: [012](./012-asset-replacement-and-provenance.md)) is
  still to build ([105](../issues/105-mpq-reader.md)).
- Today every part reads the chain directly. Once replacements exist,
  reads go through the asset resolver
  ([012](./012-asset-replacement-and-provenance.md)).

## DBC — client-side data tables

- Row-based binary table. Header: `WDBC`, record count, field count,
  record size, string-block size. Then rows, then a string block.
- The schema lives in a **layout file** (data, not code): table name, then
  each field's name, offset and type. A generator turns layout files into the
  C structs used here *and* the Lua layouts used by world-edit-to-execute, so
  the two projects cannot drift apart.
- DBCs the client needs early: `ChrRaces`, `ChrClasses`, `CharacterFacial*`,
  `SpellIcon`, `Spell`, `Item`, `ItemDisplayInfo`, `Map`, `Light`,
  `LoadingScreens`, `LiquidType`, `Talent`, `TalentTab`, `CreatureDisplayInfo`,
  `CreatureModelData`.

## BLP — textures

- BLP2 (Lich King era). 0x4 magic `BLP2`, then header, mipmap offsets,
  mipmap sizes, and pixel data.
- Compression: DXT1 / DXT3 / DXT5, or uncompressed BGRA, or palettized.
- Uploaded as `GL_COMPRESSED_RGBA_S3TC_DXT*_EXT` when the GPU supports
  S3TC, else decompressed CPU-side once at load.
- BLP1 (Warcraft III's, read since 2026-10-07, issue 107). Magic `BLP1`,
  a 156-byte header of 32-bit words, then either a JPEG header shared by
  every mipmap followed by each mipmap's own JPEG remainder (four parts:
  blue, green, red, alpha as stored, no colour conversion; read by
  `src/049-jpeg.c`), or a 256-colour BGRA palette with one index byte a
  pixel and alpha packed after at 0, 1, 4 or 8 bits. Decoded to RGBA on
  the CPU; held byte for byte to world-edit-to-execute's Lua reader by
  `tests/050-blp1-matches-lua.lua`.

## M2 — character / creature / item models

- Vertices, bones, sequences (animation clips), texture lookup, particle
  emitters, ribbon emitters, attachments, render flags.
- Companion files: `.skin` (LOD index buffers + submesh table), `.anim`
  (external animation chunks for long anims), `.phys` (cloth physics, often
  absent in 3.3.5a), `.bone`, `.skel` (Cataclysm+ — ignored).
- Skinning is bone-space: the processor poses the bones each frame (items
  hang from bones and particles start at them, so the poses are needed
  there anyway) and sends them as a one-row float picture; the graphics
  card bends the points (built, wow-chat-2026 173u). Pre-3.3.5 era models stay well under any
  reasonable bone budget.

## MDX — Warcraft III's models

- Read since 2026-11-04 (issue 401 step 2; the C twin of
  world-edit-to-execute's `src/parsers/mdx.lua`), from `war3.mpq` by
  `015-mpq.c` like any other file. One file: `MDLX`, then chunks of a
  four-letter tag and a byte count (version 800 as shipped): sequences
  in milliseconds, pictures (named, or replaceable: team colour, team
  glow, cliffs, trees), materials as stacks of layers (filter mode,
  shading flags, animated alpha), geosets (points, normals, faces,
  matrix groups, picture positions), geoset animations, nodes (bones,
  helpers, attachments, lights, emitters, ribbons, events, collision
  shapes) with their pivots, texture animations.
- `src/061-mdx.c` fills the M2 model structure (`022-m2.h`) with the
  drawable part, so one renderer draws both games' models, and keeps the
  rest (skeleton, sequences, tracks) beside it for posing (402). The
  mapping, decided in issue 401: a piece a geoset, a unit a geoset and
  layer, WC3's filter modes as M2's blends one for one, its shading flags
  as M2's material flags, replaceable pictures as their own kinds.
- Held field by field to the Lua reader by
  `tests/062-mdx-matches-lua.lua`.

## WMO — world map objects (buildings, dungeons)

- Root file describes groups (sub-meshes) and portals.
- Per-group files contain geometry, BSP, lighting, doodad references.
- Portal culling at group boundaries — the original client relied on this
  for performance inside Stormwind / Ironforge interiors.

## ADT — terrain tiles

- A continent is a 64×64 grid of tiles. Each ADT is one tile.
- A tile is 16×16 chunks (MCNK); each chunk is 8×8 quads with 9×9 outer +
  8×8 inner heightmap samples (145 verts).
- Per-chunk: textures (up to 4 blended via alpha maps), doodad refs (M2),
  WMO refs, liquid layer, shadow map, vertex colors.

## W3E — Warcraft III's ground

- A map's `war3map.w3e` (read since 2026-11-04, issue 302a,
  `src/063-w3e.c`): a head (version 11, the main tileset's letter, the
  map's ground and cliff tilesets as four-letter ids, the size in tile
  points, the south-west corner), then a grid of 7-byte tile points 128
  units apart, rows from the south: height, water level with two
  boundary bits, the ground tileset and four marks (ramp, blight, water,
  boundary), the picture's and the cliff's variations, the cliff level and
  the cliff tileset. `063-w3e.h` lays out every bit.
- Painted from tileset pictures of 4 x 4 corner-shaped pieces (8 x 4 when
  extended): each cell lays its lowest tileset whole and each higher one
  as the piece covering its own corners. Cliffs are models, not ground.
- Held field by field to world-edit-to-execute's Lua reader by
  `tests/064-w3e-matches-lua.lua`.

## WDT — world tile index

- 64×64 bitmap of which ADTs exist for a map.
- May also reference a single global WMO (for instances).

## WDL — low-resolution world

- Per-continent low-res heightmap for the distant terrain seen from a
  flying mount or the worldmap.

## Animation, sound, video

- `.anim` chunks (see M2).
- Sound: `.mp3` (music), `.wav` / `.ogg` (effects). Names referenced via
  `SoundEntries.dbc`.
- Movies: `.avi` (Bink-encoded). Cinematics. Lowest priority; can be
  stubbed for the early phases.

## Where each is consumed

| Format | First needed in       | Phase |
|--------|------------------------|-------|
| MPQ    | foundation             | 1     |
| DBC    | foundation             | 1     |
| BLP    | rendering core         | 2     |
| ADT    | world geometry         | 3     |
| WDT    | world geometry         | 3     |
| WMO    | world geometry         | 3     |
| W3E    | WC3 map mode, the editor | 3 (read), 11 |
| M2     | characters & animation | 4     |
| Sounds | audio & polish         | 10    |

Roadmap: [009-roadmap.md](./009-roadmap.md).
