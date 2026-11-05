# The Client's Data Path

*Moved here from wow-chat-2026 (its `docs/custom-client-datapath.md`) with the client on 2026-10-07
(wow-chat-2026 issue 173v). Issue numbers like 173u are wow-chat-2026's;
[009](./009-roadmap.md)'s crosswalk says which issue here continues each.*

How data flows through the custom client (issue 173), from the original
3.3.5a client's files to the picture on screen and the steps she takes.
Everything in the world is read from the original's own files. Nothing
is placed by hand. The code is in `src/`, numbered in reading order.
Each file's header comment says what it holds and lays out its formats,
and its `.info.md` lists what it offers. Every part keeps its state
behind a handle (docs/001, "Handles"); the program's own state is one
App structure in `043-main.c`, which holds every part and hands each to
the step of the frame that uses it.

For what is built and what is left, read the issues (173 and its
sub-issues). For what the client guesses and the owner checks against the
original, read `docs/046-ingame-checks.md`. For current numbers
(checks, counts), run `scripts/build --test` and the
picture options; this page doesn't copy numbers that go stale.

## 1. Sources

| Source | Read by | Holds |
|---|---|---|
| The original client's archives (`Data/*.MPQ`, newest patch first) | `015-mpq.c` | every file below, unpacked on request |
| The server's map files (`assets/server-data/maps`) | `018-maps.c` | the ground's heights the server uses (for the mover) |
| The server's navigation tiles (`assets/server-data/mmaps`) | `019-navmesh-slope.c` | the steepest walkable slope |
| The player's settings file (`assets/settings/Config.wtf`) | `014-settings.c` | the player's choices (`SET name "value"`) |
| The creature list (`assets/world/creatures-<map>.tsv`), generated from the server's world database by wow-chat-2026's `scripts/generate-custom-client-creatures` | `035-creatures.c` | where each creature stands, its look and size (the network's stand-in, 173t) |

Not in this path yet, in an ordinary run: the asset resolver
(`060-resolver.c`, issue 105a), which puts replacement packs, the
server's assets and loose overlay folders in front of the archives, with
a placeholder after them. Only a test-map run (`--test-map`, issue 1109:
a map handed in by world-edit-to-execute's editor, converted by its
W02c) opens one, with the map's folder first of the overlays, and ends
by writing a goodbye receipt the editor reads back (`067-test-map.c`,
`068-goodbye.c`). In that run the folder's `map.lua`
(`073-converted-map.c`) says which map to open and where she starts;
the client's tables are read row by row from every place that has them,
the map's rows first (`074-tables.c`, issue 106); the ground files
(WDT, ADT, WDL), the ground's pictures, the placed doodads' and
buildings' models, pictures and `.anim` files (`076-placed-models.c`
finds a placed name's files: a map's own WC3 `.mdx` from its folder,
else the `.m2` and its view) and a creature look's own WC3 model come
through the resolver (stock WC3 ones from the player's Warcraft III
install, `assetWc3`, after the game's archives; a converted map's
ground laid cell by cell with WC3's own tilesets and variations when
its build carries `war3map.w3e`, `078-wc3-ground.c`, drawn by the
ground's shader, `080-ground-shader.c`, with WC3's blight on blighted
ground, and its cliffs and ramps standing as WC3's own models among
its doodads, `079-wc3-cliffs.c`, each bent to the ground under it by
`076-placed-models.c`; its water drawn with WC3's water sheet and
ripple frames, `081-wc3-water.c`, tinted by the map's `war3map.w3i`,
`083-w3i.c`, each point laid by `082-water-prep.c`), with the magenta checker for a missing picture
and a magenta wire-frame box for a missing model; and while the server
has no map files for the map, her ground's heights are made from its
ADTs (`075-adt-heights.c`). The prefetch (034), the water (040) and the
game's own creatures (041) still ask the archives directly; each part
switches with its own issue.

## 2. From the archives to things in memory (no window needed)

| Reader | From | To |
|---|---|---|
| `016-dbc.c` | the client's tables (`DBFilesClient\*.dbc`) | rows and fields (Map, Light*, LiquidType, CharSections, ItemDisplayInfo, CreatureDisplayInfo ...) |
| `017-blp.c` (with `049-jpeg.c` for WC3's JPEG pictures) | pictures (`.blp`): the game's BLP2, and Warcraft III's BLP1 (from `war3.mpq`, read by `015-mpq.c` too) | RGBA images, at any of their smaller sizes |
| `020-adt.c` | ground files (`World\Maps\<map>\<map>_<ty>_<tx>.adt`) | chunks (heights, layers, blend maps, shadows, tints), water layers, placements of doodads and buildings |
| `063-w3e.c` | Warcraft III's ground (a map's `war3map.w3e`) | tile points (heights, water, tileset, cliff level, ramp, blight and boundary marks), then each point's heights in the world and each cell's tileset pictures and subtiles, cliff and blighted cells marked; a stroke's area redone alone (for an editor, through the FFI); drawn on a converted map's ADT ground by `078-wc3-ground.c` (each cell's own pieces, through 033, read per cell by 038's shader) |
| `083-w3i.c` | Warcraft III's map settings (a map's `war3map.w3i`) | its flags, main tileset, fog (style, heights, density, colour) and water tint; the rest passed over |
| `077-slk.c` | Warcraft III's spreadsheets (SYLK: `TerrainArt\Terrain.slk`, `TerrainArt\CliffTypes.slk`, `TerrainArt\Water.slk` ...) | each row's cells by row and column name (a tileset's picture folder and file; a cliff tileset's model folder, picture and ground; a tileset's water frames and colours) |
| `022-m2.c` | models (`.m2` + `00.skin`) | points, pieces, units (pictures, layers, blends, tints), solid shapes |
| `061-mdx.c` | Warcraft III's models (`.mdx`, from `war3.mpq`) | the same points, pieces and units as `022-m2.c` gives, and beside them its sequences, skeleton (bones, helpers, pivots) and animated values (for 402); not yet drawn by any part |
| `023-anim.c` | the same models (and `.anim` files) | skeletons, animations, poses, bent points, picture animations |
| `024-particles.c` | the same models | particle emitters, and their particles run over time |
| `021-wmo.c` | buildings (root + group files) | materials, furniture sets, parts' points and batches, solid flags, pools |
| `025-character.c` | a character's look (race, sex, choices, items) | its model with the right pieces shown, its painted skin sheet, fur, hair, cloak, and the items to hang |
| `026-light.c` | the light tables | the light here and now: sun, shade, sky, fog, water colours, skybox |
| `027-liquid.c` | ground files' water, buildings' pools | the water's surface anywhere (for the mover) |
| `028-collide.c` | models' and buildings' solid shapes, placed | floors, walls and lines of sight (for the mover and camera) |

## 3. From memory to the screen (Raylib)

Each frame, in this order (`043-main.c`):

1. **Tiles** (`refresh_tiles`): tiles round her are loaded. Each tile's
   ground file is read once, by `033-ground-prep.c` (which also works out the
   ground's points, light and blend picture, and unpacks its new
   pictures, with no window needed), and handed to:
   - `038-ground.c`: sends the prepared meshes, blend picture and new
     pictures to the card (with a converted map's WC3 cells, a small
     picture of each cell's pieces; the shader's text is
     `080-ground-shader.c`);
   - `040-water.c` (each layer's points and the shader's text from
     `082-water-prep.c`; a converted map's as WC3's, `081-wc3-water.c`)
     and `027-liquid.c`;
   - `042-doodads.c`: copies, solid shapes into `028-collide.c`, buildings'
     pools into `040-water.c` and `027-liquid.c`.
   Creatures (`035-creatures.c` finds them in the loaded tiles) are built as
   they come within 110 yards, a few a frame, and freed beyond 130
   (`041-figure.c` builds each: a creature model, or a townsperson as a
   dressed character with a baked skin).
   Tiles left behind are dropped from all of them (173s). A second
   thread (`034-prefetch.c`) reads the next ring of tiles' files ahead, into
   the archive reader's memory, and prepares their ground; the load
   takes a tile's preparation when there is one, or prepares it then
   (the start's tiles, after a jump). Preparations beyond the ring are
   let go (173u).
2. **Input and movement:**
   - the mouse drags the camera and the wheel zooms it, all the way in
     to first person (`031-camera.c`);
   - the keys and the world move her (`029-mover.c`, asking `043-main.c`'s
     ground, wall and water questions);
   - the camera follows (`031-camera.c`); in first person it sits at her
     eyes, lifted by Head Bob, and she isn't drawn.
3. **Animation:**
   - `030-moves.c` chooses her animation from what she did, and `041-figure.c`
     bends her and hangs her items;
   - the bystanders cycle theirs; the world's creatures within 100 yards
     breathe in their standing animation;
   - `042-doodads.c` bends moving doodads, poses camera-facing ones per copy,
     and runs particles near the camera.
4. **Light:** `026-light.c` gives the light at her place and hour. View
   distance and being underwater adjust the fog. Every shader is handed
   the light (`sky_light_shader`).
5. **Drawing**, back to front. The processor decides what and where;
   the graphics card bends and paints (173u, checklist:
   `docs/047-optimization-checklist.md`): pieces outside the
   camera's view are skipped (`032-sight.c`), figures and moving doodads are
   bent in their shaders from small bone pictures sent each frame,
   pictures go to the card compressed (`036-texture.c`):
   - the sky dome and skybox (`037-sky.c`, `doodads_draw_skybox`);
   - the painted ground with its shadows (`038-ground.c`);
   - her and the figures (`041-figure.c`): solid pieces, then glowing and
     see-through ones (her eyes' glow) over them, each tinted by its
     animated colour (`023-anim.c`);
   - doodads and buildings: solid, then see-through, then particles,
     which fade into the fog by distance (`042-doodads.c`, `024-particles.c`);
   - water, see-through (`040-water.c`).

## 4. The mover's questions

The mover (`029-mover.c`) only asks. `043-main.c` answers from the readers:
- **Ground:** the higher of the server's ground and a model's floor, at
  or below her feet plus a step (`018-maps.c` + `028-collide.c`).
- **Walls:** a step that hits something too steep to stand on
  (`028-collide.c`).
- **Water:** the surface above her (`027-liquid.c`). Deep enough, she swims.

## 4a. A second program: pictures of items (issue 410)

The dressing room (`058-dressing-room.c`, run by `scripts/item-pictures`)
uses the same path without the world:
- a list file in: one line per item, a name for its files and the slot
  and item look it is worn by (`45574 tabard:59981`);
- **the icon**: the item-look table names its icon picture
  (`025-character.c`), read from the archives and decoded (`017-blp.c`),
  saved as a PNG; no window needed;
- **as worn**: her look with the item over her starting outfit, built as
  a figure (`025-character.c`, `041-figure.c`, the same shader the client
  uses), posed standing, drawn front and back into a picture on the
  graphics card that is always the same size (a hidden window only
  supplies the card), saved as PNGs with see-through backgrounds;
- anything of the outfit missing from the archives refuses that line;
- out: the pictures in a folder (default `tmp/shared-memory/item-pictures/`)
  and a goodbye receipt in `output/goodbye` with the counts made and
  refused. Who uses them (wow-chat-2026's oath tabard page) is outside
  this project.

## 5. Checking

- `scripts/build --test` builds the client and the test
  program (no window) and runs every check against the original's files.
- `--asan` adds an address-checked client for hunting memory faults.
- Picture mode (`scripts/run --shot ...`) runs the world a few
  seconds and saves a picture; its options (place, time, camera, walk,
  dive, clothes, map) are listed in `scripts/run`.
