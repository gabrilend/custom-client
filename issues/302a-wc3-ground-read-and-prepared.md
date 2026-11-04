# 302a — WC3's Ground, Read and Prepared

*Parent: [302](./302-terrain-renderer.md). The reading half of the
second crosswalk's ground row ([docs/009](../docs/009-roadmap.md),
"`parsers/w3e.lua`, `assets/ground.lua`, ..."): the C twin of
world-edit-to-execute's `src/parsers/w3e.lua` and of the part of its
`src/assets/ground.lua` that works out each cell's pictures. Drawing the
ground (302 extended to paint WC3's tilesets and WoW's layers onto one
mesh, cliffs seated on it: docs/009 answer 1) is not this issue's.
Written 2026-11-04, as background work the owner asked for.*

## Current behavior

Built 2026-11-04 (branch `claude/wc3-ground-reader`, not yet merged).

- `src/063-w3e.c` / `.h`, in the reader library:
  - `w3e_read`: `war3map.w3e`'s bytes into a `W3eGround` handle: the
    head (version, main tileset and its name, custom flag, the ground
    and cliff tileset ids, the size, the south-west corner) and every
    tile point with each field the Lua reads (heights raw and in world
    units, the water's level and its boundary bits, the ground tileset,
    ramp, blight, water and boundary marks, the variations, the cliff
    level and cliff tileset). Refuses with W_ERR_CORRUPT and a sentence
    saying where: no `W3E!`, cut short anywhere, a tileset count below
    0, a width or height below 1.
  - The Lua reader's questions: `w3e_point`, `w3e_texture_name`,
    `w3e_walkable`, `w3e_tile_to_world`, `w3e_world_to_tile`,
    `w3e_stats`, and the heights in the world (`w3e_ground_z`,
    `w3e_water_z`, `w3e_is_wet`, the Lua's `ground_z`, `water_z`,
    `is_wet`).
  - `w3e_prep`: a `W3ePrep` handle with what a renderer is handed:
    each point's ground and water height as floats (the arrays the
    Lua's `map_scene.terrain_arrays` gives its `land_build`), whether
    water shows there, each cell's eight bytes of (tileset, subtile)
    pairs (the bytes the Lua's `ground.lay` gives its `land_tiles`), each
    cell's kind (textured, bare for blight, bare for a cliff) and the
    count of textured cells. Which tileset pictures are extended (twice
    as wide) is the caller's to say, as in the Lua, since only the
    pictures know.
  - `w3e_prep_area` (for [1109](./1109-the-editor-as-a-mode.md), the
    editor redrawing the ground after each stroke): after the caller
    changes a rectangle of points in place, redoes only those points
    and the cells with one of them as a corner, and says which cells.
    A 5 x 5 stroke on a 257 x 257 ground takes about 0.002 ms here
    against 3.4 ms for the whole ground.
- `scripts/build`: `063-w3e` joins the reader library (READERS).

Checked:
- `tests/044-test-client.c`, "WC3's ground (war3map.w3e, issue 302a)",
  before the archives (so it runs with no game data): a 5 x 4 ground
  written by the test, in version 11 and 12, read back field by field
  (every nibble in its place, a height stored below 0, both boundary
  marks), its heights in the world, its counts, and its 12 cells by
  hand (lowest tileset first, corner bits, extended and plain
  variations, a blighted cell and a cliff cell bare, cells on two levels
  textured beside a ramp); a point raised a level redoes exactly its four
  cells; 200 random strokes on a random 33 x 25 ground each give, redone
  by area, the bytes of a whole preparation; six cut files, a wrong
  mark, a negative count and a zero width refused with their sentences.
  2026-11-04: 44 checks, all passing, alone and in the whole program
  (built with stand-in raylib headers, as raylib isn't installed here);
  the same under the address and undefined-behaviour checkers (gcc;
  the whole program's two leak reports are its own, from stopping at
  the archives with no game data).
- `tests/064-w3e-matches-lua.lua` (LuaJIT, against `libwreaders.so`):
  - 250 grounds made by the script byte by byte: the hand-drawn one in
    both versions and with trailing bytes, random grounds from 2 x 2 to
    129 x 97 (cliffs, every bit random, tileset numbers past the list),
    every main tileset letter and two unknown ones, NaN, denormal,
    infinite and minus-zero corners, heights and water at their ends,
    no tilesets listed, one point wide; each through both readers and,
    for three patterns of extended tilesets, both preparations.
    2026-11-04: 50 identical, 197 refused by both (every shorter copy
    of the hand-drawn file among them), 3 known differences (below),
    none disagreeing.
  - an editor's strokes: 300 on the hand-drawn ground and 300 on a
    random 40 x 33 one, the same change made to both readers' points,
    the C redoing the area against the Lua laying the whole ground
    again: 600 of 600 identical.
  - the 19 maps in world-edit-to-execute's `assets/` (DAoW 2.1 to 7.5,
    Conquest, Master Crafters, War in the Plaguelands; 257 x 193 to
    481 x 481 tile points, 10 to 17 ground tilesets, all version 11),
    read through that project's archive reader: 19 identical. Some list
    17 ground tilesets, one more than a tile point can name (4 bits);
    both readers keep the 17th and no point uses it.
  - the same run with the library built under the address and
    undefined-behaviour checkers: clean.

## Intended behavior

world-edit-to-execute's Lua ground reader retires: every WC3 map's
ground reads in the W client to the numbers the Lua gives, and the
client prepares it for drawing itself, so the one ground mesh (302) can
take WC3's tilesets and the editor mode (1109) can redraw a stroke's
area alone.

## Suggested implementation steps

1. Port `w3e.lua` field for field: the same masks, the same heights
   (doubles where the Lua's are, floats where it packs floats), the
   tileset names.
2. Port `ground.variation`, `ground.cells` and the cells `ground.lay`
   leaves bare; the heights `map_scene.terrain_arrays` packs.
3. The area redo for strokes, checked against a whole preparation.
4. The test program's block, and the comparison with the Lua.
5. On the owner's machine: the comparison over the install's own maps
   (below), then retire the Lua reader in world-edit-to-execute.

## Acceptance

- Every map the comparison is given reads and prepares identically in
  both, or is refused by both (done for the 19 maps here; the stock
  maps wait, below).
- A stroke redone by area gives the bytes of a whole preparation (done).

## Decisions

Made here without the owner, for background work; each is easy to turn
round.

- **One file, reading and preparing.** Only numbers 063 and 064 were
  free: 063 is the reader and the preparation together (both window-free,
  both in the library, since world-edit-to-execute's `ground.lua` retires
  with `w3e.lua`), 064 the comparison.
- **Versions.** The Lua reads any version with version 11's layout, and
  so does the C, keeping the number for its caller to name. Version 11 is
  the only one known to exist; the checks use 11 and 12 (12 made up, to
  show both read an unknown one alike).
- **Three known differences, all on files no editor writes:** a
  tileset count below 0 (the Lua reads it as none and carries on), a
  width or height of 0, or below 0 (the Lua reads an empty ground, which
  its own `ground.cells` then can't lay). The C refuses all three as
  corrupt, by the house rule that a broken file fails loud. The
  comparison expects exactly these.
- **What the preparation adds to the Lua's:** each cell's kind (blight
  or cliff, where the Lua only leaves the cell's bytes empty), so a
  renderer can paint bare cells its own way without working it out
  again; and whether water shows at each point.
- **Left out, as not reading:** the Lua's `w3e.write` (the editor's
  saving, 1109 step 5) and `w3e.format` (a text summary); the stand-in
  tileset pictures of `ground.lua` and the cell colours of
  `map_scene.terrain_arrays` (placeholder art, the drawing side's); the
  bilinear height sampler of `map_scene` (demo code, not one of the
  twinned files; units and cliffs seated on the mesh will want it, then
  with 302).
- **Sanitizers with gcc.** clang here has no address-checker runtime;
  gcc's `-fsanitize=address,undefined` was used instead.

## Open

- **On the owner's machine** (no WC3 install here): run
  `luajit tests/064-w3e-matches-lua.lua --maps <the install's Maps folder>`
  over the stock game's own maps (campaign maps inside `.w3n` files are
  not opened by it yet), and over any map saved by Reforged's editor;
  then mark world-edit-to-execute's `w3e.lua` deprecated there (its
  `docs/wow-client-bridge.md` table could gain a row for this step).
- **Which tilesets are extended** needs their pictures: the path of each
  comes from `TerrainArt\Terrain.slk` (the Lua's `ground.path`), and the
  client has no SLK reader yet. Until then a caller says.
- **The corner bits' orientation** (south-east 1, south-west 2,
  north-east 4, north-west 8) is checked in the Lua only against its own
  stand-ins; if stock tiles show their blends mirrored once drawn (302),
  that one table changes, in both.
- **Cliff models.** Which cliff model sits on a cliff cell (from its
  corners' levels and cliff tileset) is the drawing side's, with 302's
  seating rule; nothing here chooses one.

## Related

- [302](./302-terrain-renderer.md) (the parent: drawing WC3 ground on
  the one mesh), [1109](./1109-the-editor-as-a-mode.md) (step 2: the
  ground redrawn by area after a stroke).
- [107a](./completed/107a-wc3-pictures-blp1.md): the first C twin, the
  model this one follows.
- world-edit-to-execute: `src/parsers/w3e.lua`, `src/assets/ground.lua`,
  `src/render/landscape.c` (what its renderer is handed),
  `docs/wow-client-bridge.md`.
