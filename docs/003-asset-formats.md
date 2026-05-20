# 003 — Asset Formats

Everything the client renders or plays comes from the 3.3.5a MPQ archives.
This doc inventories the formats and where each is consumed. Each format
gets its own parser module with a corresponding `.info.md` file once it lands
in `src/`.

## Container: MPQ

- Hash + block tables, optional HET/BET for larger patch archives.
- Patch chain semantics matter: later archives override files in earlier
  ones by hash. Load order (3.3.5a): `common.MPQ`, `common-2.MPQ`,
  `expansion.MPQ`, `lichking.MPQ`, locale `*.MPQ`, then `patch.MPQ`,
  `patch-2.MPQ`, `patch-3.MPQ`, locale patches.
- Implementation: StormLib.
- Asset resolution goes through the patch chain — never reads a single MPQ
  in isolation.

## DBC — client-side data tables

- Row-based binary table. Header: `WDBC`, record count, field count,
  record size, string-block size. Then rows, then a string block.
- The schema lives in our code, not the file. Each DBC has a hand-written
  C struct + reader.
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

## M2 — character / creature / item models

- Vertices, bones, sequences (animation clips), texture lookup, particle
  emitters, ribbon emitters, attachments, render flags.
- Companion files: `.skin` (LOD index buffers + submesh table), `.anim`
  (external animation chunks for long anims), `.phys` (cloth physics, often
  absent in 3.3.5a), `.bone`, `.skel` (Cataclysm+ — ignored).
- Skinning is bone-space; we evaluate the bone matrices on CPU per frame
  and upload them as a UBO. Pre-3.3.5 era models stay well under any
  reasonable bone budget.

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
| M2     | characters & animation | 4     |
| Sounds | audio & polish         | 10    |

Roadmap: [009-roadmap.md](./009-roadmap.md).
