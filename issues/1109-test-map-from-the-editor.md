# 1109 — Test Map: the Editor Opens Its Map in the W Client

*Decided 2026-10-07 as "the editor as a mode of the W client"; reshaped
2026-11-04, when the owner kept the editor's own renderer. Came from:
world-edit-to-execute's map editor (its Phase 9, issues 901-912) and its
`docs/wow-client-bridge.md`.*

## Current behavior

The map editor is world-edit-to-execute's (`src/editor/`, Lua). It runs
inside that project's scene viewer (`src/render/scene_viewer.c`, built
by `src/render/run-editor` with its models, ground, fog and interface
code), which hands Lua a small drawing surface: 2D interface pieces
(text, rectangles, frames, images, text width), model drawing, ground
building, and baked blocks of flat-shaded designs. It reads and writes
`.w3x` maps directly (not converted ones), draws the ground being edited
again after each stroke, and plays the edited map with F5 through the
same viewer, the game's rules run by that project's Lua simulation.

Step 1 is built (2026-11-04, background work the owner asked for: "build
it as best you can, answer open questions yourself, record each
decision"), stacked on 105a's resolver, which isn't on main yet:

- **`src/067-test-map.c`** reads the launch (below): the map's folder,
  the receipt's file, the camera, the realm and the account, and the
  password from `W_CLIENT_PASSWORD` only. It checks every argument,
  the client's own options too (043 hands it their list), so an option
  the client doesn't know, one short of its values, a bad camera,
  realm or account, an option given twice, or a test-map option in an
  ordinary run is refused by name. `test_map_place` makes the
  resolver's places from the settings and puts the map's folder first
  among the overlays, for this run only; the settings store and file
  are not changed.
- **`src/068-goodbye.c`** writes the goodbye receipt (below), into a
  temporary file beside it, then renamed onto it. A receipt whose
  folder isn't there is refused before any window opens.
- **`src/043-main.c`** reads the test map's options before its own,
  takes the password out of the environment once read, and from then
  on every failure of the start (the options, no `--maps` or `--client`,
  no ground under the camera, a map folder that isn't there, a missing
  archive or table) is written in the receipt with reason `error`. With
  `--camera X,Y` she starts at (X, Y), so the camera begins behind her
  there. A resolver opens with the map's folder first of the overlays
  (decision 4). Frames drawn are counted, and the receipt is written
  last, after everything is let go, with reason `closed` (Escape or the
  window closed).
- **No screens to skip yet.** This client has no login, realm or
  character screen (Phase 5's handshake and 602 aren't built): every
  run goes straight into the world, a test map's too. The realm,
  account and password are read and checked and kept on the handle for
  when they are (decision 6).
- **`scripts/run`** passes a test map's options on to the client, and
  gives `--assets assets/` for the settings' packs and overlays.

Checked (`tests/044-test-client.c`, "the test map's launch and its
goodbye receipt", 52 checks, run before the archives so it needs no game
data): the options read among the client's own (a negative `--goto`
value taken as a value); twelve kinds of refusal, each with its code and
sentence and whether a receipt can still be written; ten bad cameras,
seven bad realms, a bad account; the password not set, empty, never
named in a sentence, and no `--password` option; the map's folder
answering first through the resolver, on folders made by the test, with
the settings' overlay behind it and a pack still ahead of it; the
settings unchanged, and a saved settings file not naming the map; no
room when the settings name 16 overlays; the receipt written whole with
no temporary file left beside it, read back by LuaJIT with `loadfile`
in an empty environment, the map's path byte for byte (quotes, a
backslash, a new line, a control byte, UTF-8); an error's sentence; an
earlier receipt replaced whole; a receipt refused where its folder is
missing, taken away during the run, or its name is a folder. 52 pass,
the same under the address, undefined-behaviour and leak checkers
(`-fsanitize=address,undefined`).

Built away from the owner's machine, where raylib isn't installed, so
**043-main.c is compile-checked only**: compiled (gcc and clang,
`-Wall -Wextra -Werror`) against stand-in `raylib.h`, `raymath.h` and
`rlgl.h` holding declarations only, never committed. Its start's
failures were also run, linked with a stand-in that answers "no
window is open" and nothing else: a missing password, no `--maps`, a
bad camera and no ground under the camera each wrote their receipt
(clean under the address checker); a receipt folder that isn't there
wrote none and said why. A run that reaches the window, and so
`closed` and the frame count, hasn't been run: `scripts/build --test`
and a Test Map run on the owner's machine are the full check.

How it was before: the client had no way to be handed a map by the
editor. It took any argument it didn't know and passed over it, and an
option short of its values (`--map 1 2`) was dropped silently. Nothing
was written when it ended.

**Step 1b is built** (2026-11-04, the same background work): a test-map
run opens the map world-edit-to-execute converted, not Kalimdor. The
converter's output is its W02c's (world-edit-to-execute draft PR #243,
`src/wowbuild/`): a build folder with `World/Maps/<dir>/` (the WDT, WDL
and ADTs), `DBFilesClient/<Table>.dbc` holding only the map's own rows,
`map.lua`, `world/creatures-<id>.tsv` (035's format) and `patch-W.MPQ`.

- **`src/073-converted-map.c`** reads the folder's `map.lua` with 059's
  data reader (made public for it: `lua_data_parse`, `lua_data_read` and
  the value getters; no Lua runs): format 1, the map's number (1 to
  65535) and its folder under `World\Maps` (1 to 48 letters, digits
  and `_`, never a path), its name and area, the start (x, y, and z
  when given), the rule from WC3 positions to world yards
  (`wc3_to_wow`: scale, x0, y0), and the creature list (a path inside
  the folder). Each field is held to its type and range and a refusal
  names it and its line; fields it doesn't read are passed over, so
  the converter's later parts can add their own.
- **`src/074-tables.c`** reads the client's tables per row (106): every
  place that holds a table (packs, server folders, overlays, then the
  archives) is read once, and a row is answered by the first copy
  holding its number. The map's own rows win over the archives' rows of
  the same number, and every other row is still the archives'. Copies
  of different widths are refused, naming both. Map.dbc (043) and
  CreatureDisplayInfo, CreatureDisplayInfoExtra and CreatureModelData
  (041's `figure_from_display`) are read through it in every run; with
  no resolver (every run but a test map's) the archives are the one copy.
- **The resolver** (060) has two new kinds with no placeholder, `table`
  and `ground` (a made-up table row or ground tile would be wrong, so
  no place having one is the answer, quietly), and two new calls:
  `asset_resolve_all` (every place's copy, for 074) and `asset_read`
  (the first copy, never a placeholder, through the resolver when there
  is one, else the archives alone). The ground's readers ask through it:
  the WDT and ADTs (033, 034) and the WDL (039).
- **Heights.** She walks on the server's map files (018), which don't
  exist for a converted map until W02d writes them. **`src/075-adt-
  heights.c`** makes the server's tile from the map's own ADT the way
  the extractor does (corners and centres side by side, each chunk's
  holes as the tile's), and 018 takes it as a tile source
  (`maps_world_init_source`), so the same arithmetic answers. In a
  test-map run the server's files are used when they have the start's
  tile; otherwise the map's ADTs. With neither, the start stops with
  "no ground at the start (x, y) in map N from ..." and a receipt. The
  steepest walkable slope comes from the server's navigation tiles,
  which a converted map hasn't either: the stock maps' (1, 0, 530,
  571; the server's rule is one number for every map), said on the
  terminal.
- **WC3 models** (401 step 2, 061): a creature look whose model row
  names an `.mdx` the map brought (CreatureModelData's path as written,
  `war3mapImported\orc.mdx`) is read through the resolver; when its
  bytes start `MDLX` it goes through `mdx_read` instead of the `.m2`
  swap 041 always made. Any other name (the game's own models, which
  the converter writes with `.mdx` too) still swaps to `.m2` and reads
  the archives, as before.
- **`src/043-main.c`**: in a test-map run, `map.lua` gives the map and
  the start; `--camera` overrides the start; `--map` is refused with
  `--test-map`; Map.dbc's row (through 074) must name the folder
  `map.lua` names. `--world` defaults to `<folder>/world`, and
  `scripts/run` no longer adds its own `--world` to a test-map run. The
  archives and the resolver now open before the ground (they were
  opened after the window), since the ground reads through them. At the
  end a test-map run prints the tables' report (each table's copies and
  how many rows each answered) and how many ground tiles came from ADTs.

Checked (`tests/044-test-client.c`, "a converted map's folder", before
the archives, so no game data needed): 059's data reader on its own;
`map.lua` as W02c writes it, its optional fields left out, and
eighteen broken ones refused with the field and its line (a format 2,
an id past 16 bits or not whole, a folder that is a path or too long,
no start, a start off the world, a scale of 0, a creature list leaving
the folder ...); a made-up archive and a map folder each holding Map
and AreaTable: the map's row 900 from its folder, a row both have
answered by the map's, rows only the archives have still theirs,
copies of different widths refused (106's acceptance), a table nowhere
named, no placeholder for a table or a ground file, the same tables
without a resolver; a tilted plane written as an ADT coming back as
itself through 018's arithmetic at 400 places, a chunk's holes, a
chunk out of place refused. With a converted build folder (the third
argument, else `assets/test-map-build`; skipped when there is none),
the converter's real output, made here by running W02c's
`wow-convert.lua` on Master Crafters Final Conquest: its Map row 900
from its folder naming its folder, all seven of its tables as wide as
3.3.5a's, its start equal to WC3's start by its own rule, the ground
at the start from its own ADTs 4.167 yards (`map.lua`'s `start.z`
4.167), all 316 creatures read and standing within 0.48 yards of that
ground, and its one creature look naming its own `orc.mdx` read by
061 with something to draw. Clean under the address and
undefined-behaviour checkers.

Away from raylib, as before: every drawing part, 043 and 058 compile
with `-Wall -Wextra -Werror` against stand-in headers (never
committed), and both programs link with every one of their own symbols
found. 043's start was run on the converted folder, linked with stubs
that stop at the first raylib call: it read `map.lua`, found Map row
900 in the folder, took the ground from the ADTs and the slope from a
stock map's tile, and reached the window; a `--camera` off the map
stopped with "no ground at the start" in the receipt; `--map` with
`--test-map` was refused. Under the address and undefined-behaviour
checkers, no memory errors on any of these (a failed start leaves its
memory to the exit, as it did before). A
window has not been opened on the converted map: `scripts/build
--test` and a Test Map run on the owner's machine are the full check.

**Step 1c is built** (2026-11-05, background work the owner asked for,
the same terms: "answer open questions yourself, record each
decision"): a test-map run draws the map's placed doodads and buildings
with their own models and pictures, and its ground pictures, through
the resolver, in its order (the player's packs, the server's assets,
overlays such as the map's build folder, the player's archives, then a
placeholder). Decisions 20 to 31.

- **`src/076-placed-models.c`** finds a placed name's files (a ground
  file's MMDX and MWMO names) and reads them into the one model the
  drawer takes. A name ending in `.mdx` is first asked for as written,
  with no placeholder: bytes that start `MDLX` are a WC3 model the map
  brought (W02b copies them into the build folder at their own paths,
  `war3mapImported\Deadtree1.mdx`, with the pictures they name), read
  by 061 and its points turned into yards by `map.lua`'s
  `wc3_to_wow.scale`. Any other name (the game's own models, which
  ground files name with `.mdx` too) is the `.m2` beside it and its
  first view, `<name>00.skin`, each from the first place that has it.
  A model no place has is the placeholder box, whole (decision 23). A
  building is its root and one file per part, never a placeholder.
- **The resolver** (060) has two new calls: `asset_get` (through the
  resolver with a placeholder when there is one, else the archives
  alone, as every reader did before) and `asset_stand_in` (the
  placeholder for a name, said and counted as standing in for all of a
  model's files).
- **`src/042-doodads.c`** reads every model, picture and `.anim`
  through them, and takes the resolver from 043 (NULL outside a test
  map). A WC3 doodad is drawn in its rest pose: no skeleton, no
  particles, no solid shape; pieces with team colour and pieces hidden
  at rest are left out. A box is drawn as a magenta wire-frame; a
  building no place has is the box stretched to the size its MODF
  entry gives. 043 hands it the map's scale and prints how many WC3
  models and boxes it drew.
- **The ground's pictures** (033's `ground_prep_pixels`, and 038 for a
  tile's own) go through `asset_get`: the map's own when its folder
  has them, the checker otherwise.
- **`src/035-creatures.c`**: an empty creature list (a map with no
  units) no longer sorts a list that isn't there.

Checked (`tests/044-test-client.c`, "placed models and buildings, and
the ground's pictures, through the resolver", before the archives): a
made map folder and a made archive: a WC3 model from the folder, its
points in yards (and by WC3's own 30.72 units a yard with no rule), its
picture from the folder, its team colour kept apart and its hidden
piece at alpha 0; a game model's `.mdx` name read as its `.m2` and
view; a missing model as the box, and a model whose `.m2` is there
without its view as the box whole; a building's root and part from the
folder, a missing part, a missing building not found and the box
standing in for it; a ground picture from the folder and the checker
for one no place has; strict mode refusing the box; and with no
resolver, the archives alone with no box and no checker. With a
converted build folder ("a converted build folder"), two real
conversions by W02c's `wow-convert.lua`
(branch `claude/wete-w02c-registration`):

- **War in the Plaguelands**: its 4 ADT tiles place 11034 doodads,
  exactly the 11034 its `placements.lua` lists (4396 by its own
  models), each within 0.0021 yards of where that list says. Its 147
  models: 118 its own, read from its folder (117 with something to draw
  at rest; `EerieFog.mdx` is particles alone, 403), 28 the game's (the
  box, with no archives), 1 refused by 061 (open item below). Its own
  stand 0 to 33.33 yards tall. Their 269 pictures: 94 from its folder,
  175 the checker (WC3's own pictures, which no build folder holds),
  all decoded. Its ground tile's 17 pictures each the checker.
- **Master Crafters Final Conquest**: 2940 doodads, all where its list
  says, its 11 models all the game's.

No memory errors under the address and undefined-behaviour checkers
(the two leaks there are the test's own start, as before). Every
drawing part, 043 and 058 compile with `-Werror` under gcc and clang
against stand-in headers. A window has not been opened: what a
converted map looks like is for `scripts/build --test` and a Test Map
run on the owner's machine.

**Step 1d is built** (2026-11-05, the same background work): the
player's own Warcraft III install is a place the resolver asks, and a
converted map's ground is painted with WC3's own tilesets when the
player has them. Decisions 32 to 41.

- **The install** (`015`'s `mpq_chain_open_wc3`, 060): War3Patch.mpq,
  War3xLocal.mpq, War3x.mpq, War3Local.mpq and war3.mpq, newest first,
  found in a folder without regard to capitals and read by the client's
  own archive reader, are asked after the game's own archives and
  before the placeholder, as world-edit-to-execute's
  `docs/wow-client-bridge.md` orders it (2026-11-04: "the player's
  packs, the server's files, the player's own Blizzard install if they
  plugged it in, the baseline set, and only then the wire-frame"). The
  folder is the new setting `assetWc3` (014): one folder name under the
  profile's assets, default `wc3`, so `assets/wc3` holding the archives
  or linking to the install (decision 33). A folder that isn't there,
  or holds none of them, is said in the resolver's log and left out; an
  archive there that can't be read stops the start, naming the setting.
  So on a machine with WC3, a converted map's own WC3 models can find
  the stock pictures they name (175 of War in the Plaguelands' 269 are
  the checker without it) there; not checked here, with no install.
- **`src/077-slk.c`** (reader library): WC3's SYLK sheets, the C twin of
  world-edit-to-execute's `slk.lua`, for `TerrainArt\Terrain.slk` (each
  tileset's folder and file).
- **PR #3's `063-w3e`** is merged into this branch (decision 34).
- **`src/078-wc3-ground.c`** reads the map's `war3map.w3e` (from the
  build folder's root: the converter doesn't copy it there yet,
  decision 35), the sheet and every tileset picture through the
  resolver, and prepares the ground with 063. Painting a tile: each
  chunk's 64 cells found in the WC3 grid by map.lua's rule; the chunk's
  four most-used tilesets, drawn lowest number first; each higher
  layer's strength full where it is a cell's lowest kept tileset, its
  subtile's alpha where it is a higher one; each layer's picture its
  tileset's whole tile (decisions 36 to 39). 033 asks it for every tile
  when it is set and unpacks a WC3 layer's picture by its name, and 038
  draws the chunks as any chunk, with its own shader: no ground shader
  was needed (decision 36). 043 opens it in a test-map run and says
  what it did, or why the converter's pictures stay.

Checked (`tests/044-test-client.c`, "Warcraft III's own install, and a
converted map's ground with WC3's tilesets", before the archives): the
sheet (cells that leave their row or column out, a row named twice,
`;;`, refusals); the install's archives found by any capitals, the
patch first, an empty or missing folder; the install as a place after
the game's archives (a name both have is the game's, one only WC3 has
is WC3's patch's, every copy in order, the checker's warning naming
both), the setting's default, its refusals, a missing folder said, a
broken archive refused; a made map (17 x 9 tile points, a blighted
point, a cliff) and a made install: its tilesets, the extended one, one
missing from the sheet as the checker, a layer's whole tile, world
places to cells; a made tile painted: the two chunks on the grid only,
chunk 0 (four tilesets) exactly as WC3 lays it at all 4096 blend points
(made pictures whose subtiles' alpha covers exactly their corners, so
WC3's laying gives each quarter of a cell its corner's tileset), chunk
1 (five) folded; 033 giving a layer's name its whole tile; no install,
no `war3map.w3e`, no resolver each said. With the converter's real
output and its map's `war3map.w3e` copied into the build (as the
converter should: decision 35), and an install made for its tilesets:

- **War in the Plaguelands**: 257 x 193 tile points, 16 tilesets; its 4
  ADT tiles prepared through 033: 768 chunks painted, 256 off the WC3
  grid keeping the converter's pictures. Every one of its 49152 WC3
  cells lies in exactly one chunk cell, centre on centre (within 0.0001
  of a cell). In chunks of four tilesets or fewer every blend point is
  as WC3 lays it (962560 of 962560); over all chunks 90.4%, the rest in
  chunks of more than four, folded.
- **Master Crafters Final Conquest**: 257 x 257, 10 tilesets; all 1024
  chunks painted, all 65536 cells one to one, 4046848 of 4046848 blend
  points in chunks of four or fewer, 99.8% over all.

No memory errors under the address and undefined-behaviour checkers;
every drawing part, 043 and 058 compile with `-Werror` under gcc and
clang against stand-in headers. Not checked here: a real WC3 install
(none on this machine: its archives, its `Terrain.slk` and its tileset
pictures are made in the test), and a window.

**Step 1e is built** (2026-11-05, the same background work): a
converted map's cliffs stand as Warcraft III's own cliff models, with
the cliff's own ground under them, when the player has WC3. Decisions
42 to 50.

- **`src/079-wc3-cliffs.c`** reads `TerrainArt\CliffTypes.slk` (077,
  its columns by any capitals: decision 44) and finds every cliff cell
  of the map's `war3map.w3e`: corners on different cliff levels, none
  on a ramp (063's rule for a bare cell). Its model, as the published
  map viewers build it, is `Doodads\Terrain\<dir>\<dir><letters>
  <variation>.mdx`: the cliff tileset's `cliffModelDir`, each corner's
  level over the lowest as A, B or C (north-west, north-east,
  south-east, south-west; more than two up cut to C), and the
  south-west point's cliff variation, or the highest below it some
  place has (each asked of the resolver once), else 0 (decision 43).
  The cliff tileset is the south-west point's, else another corner's
  (decision 45). Each stands one cell east of its cell's south-west
  corner (WC3's cliff models lie west of their origin), at its lowest
  level plus its corners' mean height, unbent (decision 46; bent since
  step 1h, decision 64).
- **078** adds each cliff tileset's `groundTile` to its tilesets (when
  the map's list lacks it) and lays a cliff cell with a model whole
  with that ground (decision 47; decision 39 kept for blight, and for
  a cliff cell with no cliff tileset or no ground). Its new
  `wc3_ground_place_cliffs` adds a tile's cliffs to its doodads: each
  model's name once, each cliff a placement with its own id (from
  `WC3_CLIFF_UID`, past the converter's) on the tile its cell's centre
  is on, turned as the converter turns a WC3 doodad facing 0 (decision
  42). 033 calls it beside the painting, so the reading-ahead thread
  places cliffs too.
- **076**: a placed name may carry `|11=<picture>` after its model, and
  a WC3 model's replaceable picture 11 (the cliff's) is drawn with that
  file; so 042 draws a cliff as any WC3 doodad, from the install, with
  its tileset's cliff picture (decision 49). 043 says how many cliffs
  stand, how many models were found, and how many cells are left (no
  cliff tileset, ramps).
- **Ramps are next** (decision 48): their cells have no model yet; the
  converted ground's slope shows there, laid with 063's pictures.

Checked (`tests/044-test-client.c`, "a converted map's cliffs as
Warcraft III's own cliff models", before the archives): letters, cut to
C; the sheet's columns by any capitals; a made map (9 x 9 points: a
plateau, a two-level point, a steep one, a ramp, corners with no cliff
tileset, a cliff tileset the sheet lacks, variations asked) with a made
install (both sheets, three tileset pictures, three cliff models made
with a replaceable 11): every count, cell (2, 2)'s model name and
origin, cell (5, 5)'s variation falling back to 0, a corner's tileset
taken when the south-west names none, the ground under the cliffs
added as a tileset and laid whole (all 4096 blend points as expected),
every cliff placed in its tile once with its own id and turn, none in
another tile, and 076 reading a placed name with the cliff picture put
in for replaceable 11 (a bad mark refused). An install with no
`CliffTypes.slk` keeps decision 39. With the real conversions'
`war3map.w3e` (copied in by hand; world-edit-to-execute's draft PR #246
makes `convert` copy it) and an install made for their tilesets and
cliff tilesets, with every cliff model they need in variation 0 as
loose files:

- **War in the Plaguelands**: 498 cliff cells, all placed (cliff
  tilesets CYdi, CYsq), 47 models, all found; 331 asked for a later
  variation take 0. Over its 4 tiles each cliff placed once, on its
  tile, its origin within 0.05 units of one cell east of its cell's
  south-west corner, its name by its letters, folder and picture. Its
  foot is on its own level: at most 2.56 yards below its cell's lowest
  converted corner (less than a level's 4.17), never above its highest.
  The ground with cliff cells on their ground: 950272 of 950272 blend
  points as expected in chunks of four tilesets or fewer, 90.4% of all.
- **Master Crafters Final Conquest**: 2123 cliff cells (CAgr, CAdi),
  2002 placed, 121 with no cliff tileset on any corner (kept as
  before), 980 ramp cells left for later; 73 models, all found; each
  placed once, named and placed right, at most 2.36 yards below its
  lowest corner; 4001792 of 4001792 points as expected, 99.8% of all.

No memory errors under the address and undefined-behaviour checkers;
every drawing part, 043 and 058 compile with `-Werror` under gcc and
clang against stand-in headers. Not checked here: real WC3 cliff models
(their size, that they lie west of their origin, which variations
exist), a real `CliffTypes.slk` (its column names), and a window.

The tests' arguments: the build folder is the third and Warcraft III's
`war3.mpq` (for the archives' WC3 checks) now the fourth; both were the
third before (decision 50).

**Step 1f is built** (2026-11-05, the same background work): ramps
stand as WC3's own ramp models, and blighted ground wears WC3's blight,
when the player has WC3. Decisions 51 to 56.

- **Ramps (079):** a cell on different levels with a ramp corner gets
  a model from its cliff tileset's `rampModelDir` (`CliffTrans` when the
  sheet has none), named as a cliff's but with a ramp corner's letter
  `L` on the lowest level and `H` one up (`HLLH`: down to the east),
  and placed, varied and pictured by the cliffs' rules. The ramp's
  lower, flat half gets none (decision 51). A ramp's cell keeps its own
  ground (decision 52). The counts keep ramps apart; 043 says both.
- **Blight (078):** `UI\WorldEditData.txt`'s `[TileSets]` row for the
  map's main tileset names its blight picture (second field, `.blp`
  added: decision 53); it becomes a tileset of its own, the last, and a
  blighted cell is laid as WC3 lays tilesets with the blight on each
  blighted corner (decision 54). With no such file or row, blighted
  cells are laid as before.

Checked (the same section of `tests/044-test-client.c`): ramp letters;
the made map's ramp (four cells, all placed, cell (6, 3)'s
`CliffTransAHAA0`, found in the install) and its two blighted points
(the editor's data with other sections and rows round the one asked,
`A = ` spaced; the blight picture from the install; four blighted cells
laid with it, all 4096 points as expected, its layer full at a
blighted corner). With the real conversions, an install made with
`rampModelDir`, the editor's data and a blight picture, and every model
they need as loose files:

- **War in the Plaguelands**: no ramps; 1246 blighted points laid with
  the blight; 925696 of 925696 points as expected in chunks of four
  tilesets or fewer (89.5% of all, the blight a fifth tileset in some).
- **Master Crafters**: 980 ramp cells, 905 placed (75 with no cliff
  tileset on any corner), among 2907 cliff and ramp models placed once
  each, on their tile and cell, named by the rule (186 models); 184
  blighted points; 3932160 of 3932160 points as expected (99.7%).

No memory errors under the address and undefined-behaviour checkers.
Not checked here: real ramp models (their names, that one reaches over
the ramp's flat half), a real `WorldEditData.txt` (that the blight is
its second field), and a window (open question 10).

**Step 1g is built** (2026-11-05, the same background work): a
converted map's WC3 ground is laid cell by cell, as WC3 lays it, by the
ground's shader. Decisions 57 to 63.

- **The cells (078, 033):** `wc3_ground_lay_tile` gives each of a
  tile's 16384 cells its pieces, lowest first: each a picture of the
  tile (a tileset's picture as a square of 4 x 4 subtiles, or each half
  of an extended one) and the subtile in it, the lowest its south-west
  point's variation's (decision 60). 033 keeps them with the prepared
  tile and packs them for the card (`ground_prep_cell_texels`: 256 x
  128, each cell's layers and subtiles), and 038 sends them as a small
  picture a tile, beside its blend picture.
- **The shader (080, 038):** moved out of 038 into its own window-free
  file (decision 58). A tile with cells reads, at each point, its cell
  (its chunk from the blend square it is in, the cell from the repeat),
  draws the cell's lowest piece whole and each higher one over it by its
  own alpha; any other tile, and a chunk off the WC3 grid, as before. So
  a chunk shows every tileset its cells lay, however many, and each
  cell its own variation and the pieces' own colours (decisions 57, 59).

Checked (`tests/044-test-client.c`, in the WC3 ground and cliff
sections, before the archives): the made pictures now colour each
subtile its own (decision 62); each tileset's layers (an extended
one's two halves); the made map's two chunks (one laying five
tilesets) cell for cell and point for point as WC3 lays them, 121 of
128 cells with another variation; the cells packed for the card; the
shader's text reading them as packed. With the real conversions and
the made installs:

- **War in the Plaguelands**: every one of its 49152 cells' pieces as
  WC3's rule gives them (42050 laid with a variation other than subtile
  15, 7873 of them an extended picture's), and 3145728 of 3145728 blend
  points as WC3 lays them as the shader reads the cells (before: 90.4%,
  folded to four tilesets a chunk; 531 of its chunks lay more than
  four); with cliffs, ramps and blight, 3145728 of 3145728 (89.5%
  before). Each cell's centre point finds its own cell by the shader's
  reckoning from 033's texture places.
- **Master Crafters**: all 65536 cells, all 4194304 points (99.8%
  before; 99.7% with cliffs and blight), 36 chunks laying more than
  four.

The shader itself, outside the tests (decision 63): compiled and linked
on Mesa's software renderer (llvmpipe, GL 4.5 core) under a virtual X
display, and one made chunk drawn through it 512 x 512 (three made
tileset pictures in the array, a made cell table packed by 033, 033's
point layout): every pixel of its 63 WC3 cells as the rule lays them,
read nearest and smoothed with smaller copies, and its one cell with
no piece as its chunk's own layer. No memory errors under the address
and undefined-behaviour checkers; every drawing part, 043 and 058
compile with `-Werror` under gcc and clang against stand-in headers.
Not checked here: real WC3 tileset pictures (that subtiles 0 and 15
are whole tiles, open question 11), Raylib binding the cells as the
material's second map, and a window.

**Step 1h is built** (2026-11-05, the same background work): a
converted map's cliffs and ramps bent to the ground under each of their
points, as WC3 bends them, and the first step of WC3's own water.
Decisions 64 to 69.

- **The bend (079, 076, 078):** each cliff and ramp keeps the ground's
  heights round its cell: the 4 x 4 tile points from one west and one
  south of its south-west corner to two east and two north (clamped at
  the grid's edge), each less its corners' mean height, in sixteenths
  of a WC3 unit (exact: heights are in quarters). 079 hands it out in
  the model's name (`wc3_cliffs_name`: `<model>|11=<picture>|bend=` and
  the 16 numbers, rows from the south), and 076 raises each of the
  model's points by the ground under it, blended between the four grid
  points round it, before the model is scaled to yards. A cliff on even
  ground keeps its plain name, so its model is still shared (decisions
  64, 65; decision 46's "unbent" lifted).
- **The water (081):** a new window-free reader, `081-wc3-water.c`,
  reads WC3's water sheet (`TerrainArt\Water.slk`) row for the map's
  tileset (its letter and `Sha`), every frame of its ripples
  (`texFile`, two digits, `.blp`) through the resolver, and the
  colours shallow and deep water go between; it gives each wet tile
  point its depth and its colour by WC3's rule, and the frame shown at a
  time. Nothing draws with it yet: 040 still draws the converter's
  water with the game's lake pictures and the zone's colours. The rest
  is the water plan (decision 67).

Checked (`tests/044-test-client.c`, in the cliff section and a new
water section, before the archives): a made cliff model's square bent
by a made grid, point for point, either order of the name's marks, a
short or overlong bend refused, a bent name no place has the box
unbent; the made map's cliff on uneven ground named with its bend, a
ramp on the grid's east edge with its clamped points. With the real
conversions, each cliff's and ramp's foot read back through 076 at its
corners on its lowest level:

- **War in the Plaguelands**: 206 of its 498 cliffs and ramps bent;
  over 988 corners its foot is at most 0.0000 yards off the converted
  ground (unbent: up to 4.18).
- **Master Crafters**: 1011 of 2907 bent; over 5806 corners at most
  0.0000 yards off (unbent: up to 23.33).

The water with a made install: the row by the tileset's letter, three
frames (the map's own second frame before the install's), the frame
shown, each made point's depth and colour, the colour rule at its
edges, and the refusals (no sheet, no row, a missing frame, numTex 0, a
colour past 255, no texFile). With the real conversions' `war3map.w3e`
and a made sheet: War in the Plaguelands' 4627 wet points (768
shallow, 3859 deep, the deepest 2.52 cells) and Master Crafters' 14313
(6189 shallow, 8124 deep, the deepest 7.76), each point's depth and
colour as worked out in the test from its stored numbers. No memory
errors under the address and undefined-behaviour checkers. Not checked
here: real cliff and ramp models bent (their points' places, decision
46's cell east), their normals (left as the model has them, so a bent
face is lit as if flat), real WC3 water (open question 12), and a
window.

**Step 1i is built** (2026-11-05, the same background work): a
converted map's water drawn as WC3 draws it, tinted by the map's own
`war3map.w3i`, and the converter's water reconciled with WC3's: the
water plan's steps 2, 3 (this client's half) and 4. Decisions 70 to 75.

- **Drawn (040, 082, 043):** in a test-map run with WC3's water (081),
  every water and sea layer of the converted ground is drawn with WC3's
  own frames (at the sheet's rate) and each point's own colour by its
  depth: the picture times that colour, alpha too, unlit, fogged as
  the rest of the world. Lava, slime and buildings' pools keep the
  game's look. The shader's text moved to a window-free file,
  `082-water-prep.c`, with the layer's points (`water_prep_layer`), so
  both are tested without a window (decision 71).
- **Tinted (083, 081):** a new reader, `083-w3i.c`, reads the map's
  `war3map.w3i` (versions 18, 25, 28, 31 and later; only its flags,
  tileset, fog and water tint) from the build's root, where
  world-edit-to-execute's convert copies it since its draft PR #247.
  With the tint's flag (0x10000) each colour is the rule's times the
  tint (decision 72). The fog is read, not used yet.
- **Reconciled (082):** each point on a WC3 tile point stands at that
  point's own surface, so a dry corner of a wet cell sinks under its
  bank as in WC3, where the converter drew it at the wet corner's
  level; water the converter adds past the map's edge keeps its height
  and takes the nearest point's colour (decision 73).

Checked (`tests/044-test-client.c`, in the water section, before the
archives): the w3i reader on made files of versions 18, 25, 28 and 31
(each field; the tint used only with its flag; a file cut anywhere
before the tint's end refused, naming the field; versions 17, 20 and 24
refused); the made map tinted by its own `war3map.w3i`, untinted
without its flag, refused when unreadable; a world place to its point;
a made layer's 24 points, plain and as WC3's (the bank's corner sunk:
2 points moved; 3 points past the grid kept); the shader's text. With
the real conversions and their real `war3map.w3i` copied in as the
converter now copies it:

- **War in the Plaguelands**: its w3i (version 25, tileset Y, flags
  0x1fc7a) tints its water (50, 255, 255, 255 read red first: open
  question 13); its fog is style 0 (none; since step 1j read as
  linear, turned on by its flag 0x2000: decision 76). Its 5463 wet cells on the
  grid are exactly WC3's cells with a wet corner (none either side's
  alone), its surface WC3's at every wet corner; 2624 more wet cells
  lie past the grid's south edge at its sea level. Its 223 layers'
  48522 points as 040 draws them: 32901 on a tile point at WC3's
  surface and colour (none moved), 15621 past the grid.
- **Master Crafters**: its w3i (version 25, tileset A, flags 0xdc78)
  sets no tint. Its 15648 wet cells are WC3's exactly; of its 358
  layers' 93888 points, all on tile points, 31 at dry corners move
  from the converter's height to WC3's surface.

These w3i fields match world-edit-to-execute's Lua reader on both maps
(flags, fog heights, density, the tint's four bytes). No memory errors
under the address and undefined-behaviour checkers. Outside the tests
(decision 63's way): the water shader compiled and linked on Mesa's
software renderer (llvmpipe, GL 4.5 core) under a virtual X display,
all 13 uniforms 040 and the sky set active; two made triangles drawn
256 x 256 with a made frame and four made point colours, blended over
a made background, matched as worked out in C on every pixel away from
a texel's edge and the diagonal (36672 of 36672) for WC3's water
unfogged (worst 2 of 255) and fogged whole (worst 1), lava (exact)
and the game's own water (worst 1). Not checked here: real WC3 water
frames and sheet (open question 12), a real tint's look (open question
13), and Raylib drawing it in a window.

**Step 1j is built** (2026-11-05, the same background work): a
converted map's own fog, from its `war3map.w3i`, drawn in test-map runs
in place of the zone's, over the ground, the water, the doodads and the
figures alike. Decisions 76 to 80; open question 13 updated with what
the repositories hold.

- **Read and turned into yards (084, 083):** a new window-free file,
  `084-map-fog.c`, reads the map's `war3map.w3i` through the resolver
  (`map_w3i_read`, which 081 now uses too, so the file is read one way)
  and makes its fog (`map_fog_from_w3i`): on when the editor's "use
  terrain fog" flag, 0x2000, is set (083's new `fog_on`, decision 76);
  its start and end, distances from the eye in WC3 units, times the
  map's yards per unit (decision 77); its colour red, green, blue over
  255; every style drawn linear (decision 78).
- **Drawn (043):** each frame, after the zone's light is read, the
  map's fog takes its colour and distances, unless the eye is under the
  water, and the view distance still ends it at the latest
  (`map_fog_frame`, decision 79). Every drawer takes its fog from that
  one light (the ground 038/080, the water 040/082, the doodads and
  their particles 042, the figures 041, the horizon 039, the clear
  colour), so no shader changed. The start says which fog is used and
  why.
- **Kept the zone's fog:** a map whose `war3map.w3i` turns no terrain
  fog on, a Reign of Chaos file (no fog in it), and a build with no
  `war3map.w3i`; one 083 can't read stops the run, naming it (decision
  80).
- **Fixed on the way (059):** an empty folder as a place handed a null
  list to `qsort` and `bsearch`, undefined behaviour the
  undefined-behaviour checker caught in the new test of a map folder
  with nothing in it; an empty folder now sorts and finds nothing.

Checked (`tests/044-test-client.c`, a section of its own after the
water, before the archives): made files with the flag and styles 0, 1
and 2 (on, its distances 3500 to 5000 units as 113.9 to 162.8 yards at
1/30.72, its colour red first; each style the same distances); without
the flag, a Reign of Chaos file and no file (the zone's, said why); a
start before the eye, an end before its start, a scale of 0 and a
distance that isn't a number; a frame's light out of the water (the
map's fog, every other field the zone's as it was), under it (the
zone's, untouched), with a view distance short of the fog (brought in,
the start by the same share), and with no map fog (as before); the file
through a resolver from the map's folder (none, read, refused). With
the real conversions and their real `war3map.w3i`:

- **War in the Plaguelands** (flags 0x1fc7a, the fog's flag set): its
  own fog, style 0 (linear), 3500 to 5000 units, 113.9 to 162.8 yards,
  colour 30, 120, 255.
- **Master Crafters** (flags 0xdc78, no fog flag): the zone's fog.

No memory errors under the address and undefined-behaviour checkers.
Outside the tests (decision 63's way): the four shaders that fog the
world compiled and linked on Mesa's software renderer (llvmpipe, GL 4.5
core) under a virtual X display: the ground's (080) and the water's
(082) as linked in, the figures' (041) and the doodads' (042) fragment
shaders read from their source files' text. Each drew two made
triangles from 40 to 303 yards from the eye, 300 x 80, with War in the
Plaguelands' fog from its real file through 083 and 084 over a made
zone light, the five uniforms set as `sky_light_shader` sets them; every
one of 23404 checked pixels per shader (6554 before the fog, 4368 in
it, 12482 lost in it) matched the picture's colour faded to the fog's
as worked out in C (worst 0 of 255), WC3's water, the figures' solid
and added-light modes and the doodads alike. Not checked here: the
map's fog as WC3 draws it (open question 13), the exponential styles'
look (decision 78), and Raylib drawing it in a window.

**Step 1k is built** (2026-11-05, the same background work): the
baseline set's place in the resolver, and the smallest set, made from
the converter's own tables. Decisions 81 to 88.

- **The place (060, 014):** the resolver asks a fifth place after the
  Blizzard installs (the game's archives, then Warcraft III's) and
  before the placeholder, the owner's order (2026-11-04: "the player's
  packs, the server's files, the player's own Blizzard install if they
  plugged it in, the baseline set, and only then the wire-frame"). It
  is a folder laid out as a pack, its `manifest.lua` read by 059 as a
  pack's, named by the new setting `assetBaseline` (one folder name
  under the profile's assets, default `baseline`): not there, said and
  left out; there with no manifest or a broken one, the start stops,
  naming the setting (decisions 81, 82). Its answers are their own
  place (`ASSET_FROM_BASELINE`, "baseline" in the trace and a column in
  the counts table), not placeholders: no warning, strict mode takes
  them, and none counts as replaced past the threshold. A placeholder's
  warning names the set among the places when it is there (decision
  83). `asset_resolve_all` gives its copy last.
- **The water through the resolver (040):** its frames are asked with
  `asset_read` (never a placeholder; a run ends at its first missing
  frame, as before), so a pack, an overlay or the set can answer them.
  It was one of the parts still reading the archives directly.
- **The smallest set (085, 086):** nothing in either repository says
  what the set holds, beyond world-edit-to-execute's
  `docs/wow-client-bridge.md` (the asset forge, W05 and W06, will make
  it) and the converter's `models.lua` (its WoW paths are "the names
  the baseline set has to cover"), and neither tracks any art. So it is
  made by code from the converter's tables (decision 84):
  `ground_textures.lua`'s every ground, blight and fallback path as a
  64 x 64 picture of the row's own `rgb`, mottled and seamless, the
  same path always the same picture; `models.lua`'s every doodad `.m2`
  path (types, by model, classes) and its `00.skin` answered by one
  stand-in, a grey eight-sided post a yard across and two high, with
  its own picture `Baseline\StandIn.blp` (decision 85); LiquidType 1's
  run of 30 water frames, `XTextures\river\lake_a.1.blp` onward
  (decision 86). Each file at its folded name with a manifest entry
  (its SHA-256, route `hand`, lineage `independent`, no scores). A new
  program, `make-baseline`, writes it (`scripts/build` builds it;
  `scripts/run` runs it once when `assets/baseline` isn't there and
  the tables are, under `WETE`; decision 87). From the converter's
  tables (world-edit-to-execute's branch with W02's `wowbuild`): 179
  ground pictures, 47 doodad paths on the post (24 paths named twice,
  38 creature rows left out), 30 frames: 212 files, 304 entries, 4.4 MB.

Checked (`tests/044-test-client.c`, a section of its own after the
fog, before the archives): SHA-256 against FIPS 180-4's examples; a
ground picture decoded by 017 (64 x 64, its colour the row's on the
whole, mottled, opaque, no seam), the same seed the same bytes; the
water's 30 frames decoded, each a step on, the run repeating; the post
read by 022 (41 points, 24 triangles all wound outward, one piece, one
unit drawing its named picture, standing on the ground two yards
high); a set made from tables written here (every count, the manifest
read by 059, every file hashing to its id, made again over itself) and
its refusals (a folder of someone else's files, an `rgb` of two
numbers, past 255 or not whole, with file and line; a `wow` empty, not
text, climbing out of the folder or naming the manifest; a table not
there); the setting (its default, a list or `..` refused, empty for
none); and the resolver with it: a picture no install has from the set
with its entry, one both have from the install first, every copy with
the set's last, a doodad named `.mdx` as a ground file names it drawn
as the post (not the box) with the post's picture from the set, a
row's `00.skin`, the water's last frame through `asset_read` and one
past it not found, another kind refused naming the set, a creature
still the box with the set named in the warning, the counts and the
report's column, strict mode taking its answers, a folder not there
(said; the checker as before), and a folder with no manifest or a
broken one stopping the start. With the converter's real tables and
the real conversions, no install:

- **War in the Plaguelands:** its 4 tiles' 17 ground pictures all from
  the set, its 147 doodad models the set's post (28) or its own WC3
  models (118; one, `war3mapImported\glow.mdx`, refused by 061 as
  before), its 223 water layers' 30 frames from the set.
- **Master Crafters:** 11 ground pictures and 11 doodad models, all
  from the set; 358 water layers' frames from the set.

No memory errors under the address and undefined-behaviour checkers.
Not checked here: the water frames' names against a 3.3.5a client
(recalled: decision 86), the set drawn in a window, and a run with no
WoW install, which still stops at the start: the client's tables
(`LiquidType`, `Light`, `Map` ...), her model and the creatures come
from the archives (decision 88; since step 1l, it starts).

**Step 1l is built** (2026-11-05, the same background work): a
test-map run starts with no WoW install at all. Decisions 89 to 97.

- **The start, out of the window (087):** what 043 did before its
  window opens (the archives, the resolver, the tables, the map's row,
  its heights, her start height and the steepest walkable slope) is
  now `world_start_open`, so the test program runs it as 043 does. In
  a test map, no archives in the client's folder is said ("no WoW
  install: ...") and the start goes on with an empty chain, the map's
  own folder and the baseline set answering; an ordinary run still
  stops, naming the folder (decision 89). No server navigation tiles:
  the server's rule, 60 degrees, as its tiles for Kalimdor hold it
  (decision 92).
- **The tables a start needs, from the set (085, 074, 026, 040):** the
  set now holds `LiquidType` (one row, 1 "Water", its frames the set's
  own run) and `Light`, `LightIntBand`, `LightFloatBand`,
  `LightParams` and `LightSkybox`, each as wide as 3.3.5a's and empty
  (decision 90). 026 reads its tables through the resolver (the first
  copy whole: no light, so 043's plain grey day); 040 reads
  `LiquidType` per row through 074, the map's own rows first; a set's
  copy of another width than the first is left out, said, never a
  refusal (decision 91). `Map`, `AreaTable`, `CreatureDisplayInfo`,
  `CreatureModelData`, `Faction`, `FactionTemplate` and
  `LoadingScreens` already come from the map's own folder.
- **Her and the creatures (085, 088, 041, 043):** the set holds a
  stand-in figure, `Baseline\StandInFigure.m2`, an eight-sided sand
  column a person's height with a blue face looking the way a model
  faces, one still bone and one animation, Stand (decision 93). 088
  says which files a creature's display row means (the map's own WC3
  model, the game's `.m2` and view, a character look, or the stand-in
  where no place has the model, or only the set's skeleton-less post
  does); 041 builds from it and reads every file (models, `.anim`
  files, pictures, an item's model) through the resolver (decision 94).
  Her character needs `CharStartOutfit`, `CharSections` and the game's
  models, so in a test map with none the stand-in stands in her place,
  said, and stands while she moves (Stand only).
- **`scripts/run`:** a test map with no server map folder or no
  `common.MPQ` is a note, not an error (decision 96).

Checked (`tests/044-test-client.c`, "a test-map start with no install",
after the baseline set, before the archives): the set's six tables'
widths and `LiquidType`'s row; the figure read by 022 (41 points, 72
indices, its picture), 1.8 yards high, its blue face along +x, its
skeleton read by 023 with no `.anim` asked for and posed still by 030,
its picture's colours; 074 per row (the map's row, then a row only the
set has) and a set's copy of another width left out; a creature whose
model only the set's post answers given the figure; an ordinary run
with no install refused. With the converter's real tables and builds,
and no install, no server files (`/nowhere`):

- **War in the Plaguelands:** 087 opens map 900 on its folder from its
  own `Map` row, its heights from its own ground files, the start at
  -0.659 yards (map.lua -0.659), 60 degrees; the light tables empty;
  `LiquidType` 1 and its 30 frames from the set; her character not
  made (`CharStartOutfit`), the stand-in read in her place; its
  `war3map.w3i` read, WC3's ground not found; the start tile's 17
  pictures prepared, none the checker. It has no creatures.
- **Master Crafters:** the same (start 4.167 yards, 10 pictures), and
  its 316 creatures' 19 looks: 1 its own WC3 model, 18 the stand-in
  figure, all readable.
- With no set as well, the start opens on the map's own files and
  stops at the light tables, named.

No memory errors under the address and undefined-behaviour checkers;
043 and the drawing parts compile under gcc and clang with warnings as
errors. Not checked here: the window, so nothing drawn (the figure
uploaded, the grey day, the ground), and a whole run under
`scripts/run`; what still needs an install is decision 97.

**Step 1m is built** (2026-11-05, the same background work): nothing in
a test-map run reads the archives past the resolver. Decisions 98 to
101.

- **Her character through the resolver (025):** her tables
  (`CharStartOutfit`, `ItemDisplayInfo`, `CharHairGeosets`,
  `CharacterFacialHairStyles`, `CharSections`, `HelmetGeosetVisData`),
  her race's model and view, and every picture on her (skin, face,
  hair, fur, underwear, the items' region pictures, the cloak) are
  asked of the resolver when one is open, else of the archives alone,
  as before. Each table is read whole, the first place's copy (they
  are scanned by race, sex and choice, not asked by number; decision
  98). A pack or the map's folder can now hold a character; with no
  install and no such place she is still the stand-in figure (step
  1l). A build that fails part way lets go of what it made (decision
  100).
- **The read-ahead through the resolver (034):** each tile's pictures,
  doodads (a converted map's own WC3 model as named, MDLX inside, with
  its pictures; else the `.m2`, its view and pictures) and buildings
  (root, parts, pictures) are asked as the tile's load asks them,
  never as a placeholder (decision 99).
- **What still reads outside it, and why (decision 101):** 015 is the
  archive reader itself; 060 reads the chain as the Blizzard place;
  087 opens the chain (empty with no install); 058, the dressing room,
  is its own program with no resolver and needs an install (its item
  icons). The server's map files and navigation tiles (018, 019), the
  creature list (035) and the settings file (014) are read from their
  own folders: they are not the game's archives.

Checked (`tests/044-test-client.c`, in "a test-map start with no
install"): a character made from one overlay folder with no archives
(its tables written here, the stand-in figure as a night-elf woman's
model, its skin picture): a starting outfit, an item's icon, the
character built with its worn chest, one whose model no place has
refused with nothing left to free, an outfit the table hasn't refused
as from the archives, and with neither resolver nor archives not
found, naming the table; and with the real builds and no install, the
read-ahead on her start tile reading every file through the resolver:
War in the Plaguelands' 224 (its ground file, 17 pictures, 93 doodads
and theirs), Master Crafters' 29 (10 pictures, 6 doodads), none from an
archive (before this step, its ground file alone). An ordinary run's
reads are as before (no resolver: the archives alone); its checks
against the archives were not run here (no client).

**Step 1n is built** (2026-11-05, the same background work): a test map
runs with no window, and step 5's check runs on it. Decisions 102 to
110. The editor's side of F5 (step 2) is not here.

- **The option (067):** `--headless MOVES`, a test-map option like
  `--camera`: refused outside a test-map run, and with `--shot`
  (decision 102).
- **The moves (089, 090):** a Lua data file of format 1, read by 059's
  data-only reader as `map.lua` is: `format`, an optional `name` and
  `step` (seconds, 0.01 to 0.1, default 0.05), and `moves`, each
  `{ hold = "forward left", seconds = 1.5, walking = true }`, holding
  any of forward, back, left, right, strafe_left, strafe_right, jump
  and swim_down. A field or key it doesn't have is refused by name and
  line; at most 1000 moves and an hour of steps (decisions 103, 104).
  The project's own list is `tests/090-test-map-moves.lua`: 14 moves,
  24 seconds, 480 steps (a run, curves, jumps, turns, strafes, a walk,
  standing).
- **The run (089):** `headless_run` opens the world as 043 does
  (`world_start_open`), finds her figure (her character from 025,
  window-free, else the baseline set's stand-in) and each creature
  look (088, read as 041 would), then steps the mover (029) with the
  fixed step from the map's start, facing and speed as 043's (now
  shared in 087: decision 110), over the map's ground and the water of
  the tiles round her (each tile's ground file read through the
  resolver for its water; no models' floors or walls: decision 105),
  choosing her animation from the same keys as 043 (030; decision
  106). Then the receipt (decision 107).
- **The receipt (068):** `goodbye_count` adds named numbers after its
  fields, exactly (`%.17g`), format still 1: moves, steps; her_stand_in,
  her_vertices, her_bones, her_sequences, her_animation_changes,
  her_animation; creature_spawns, creature_looks, looks_wc3, looks_game,
  looks_stand_in, looks_character, looks_missing, looks_unreadable;
  start_x/y/z, end_x/y/z, end_facing, distance, lowest_z, highest_z,
  tiles, ground_files, steps_airborne, steps_swimming, steps_sliding,
  steps_no_ground. `frames` stays 0 (nothing drawn); `reason` is
  `closed` when the moves end, `error` with `why` when the start fails.
  The client exits 0, 1 on an error, 2 on a bad option.
- **043:** after the settings, a headless run hands its start to
  `headless_run` and ends with its status; no window opens.
  `scripts/run` passes `--headless` on, and its header says so.

Checked (`tests/044-test-client.c`, in "a test-map start with no
install"): the project's moves read (14, 480 steps); nine faulty move
files refused, each naming what is wrong; a move shorter than a step
takes one; the receipt's counts (exact, a name given again replacing
its value, bad names and non-finite values refused, none after
writing) and LuaJIT reading the counted receipt as the editor does;
the option with and without a test map. With the real builds and no
install, run twice on the same build (step 5, decision 108):

- **War in the Plaguelands:** 480 steps, the stand-in for her, no
  creatures (its list is empty), 125.7 yards, ending at (477.46, 72.38,
  0.89), 47 steps in the air, none with no ground; the two receipts
  the same but for their times.
- **Master Crafters:** 480 steps, 316 creatures' 19 looks (1 its own
  WC3 model, 18 the stand-in), 141.6 yards to (-511.03, -438.39, 4.17),
  45 in the air; the two receipts the same.
- Other moves give another receipt (the comparison finds the
  difference), and a start that fails writes `error` naming what
  stopped it, with no counts.

Not checked: the client binary itself run headless (043 links raylib,
which isn't here: its branch compiles under gcc and clang, and the
test program runs the same `headless_run`), two separate processes
compared (the test runs both in one), and a windowed run's moves (not
repeatable: its steps follow the frame time; decision 108).

**Step 1o is built** (2026-11-05, the same background work): the
headless world has the placed models' floors and walls, and two routes
reach water and the map's edge. Decisions 111 to 117 (111 closes
decision 105's gap).

- **The models' shapes with no window (091):** the shape of one model
  (a game model's collision lists; none for a WC3 model, the box or the
  baseline set's stand-ins) and of one building part (its triangles
  marked solid) moved out of the doodad drawer (042) into
  `src/091-model-solids.c`, which 042 now calls, so both runs build them
  the same way. 091 also places every model and building a tile lists,
  as 042 does (read once a name through 076; a placement on two tiles
  placed once; furnishings of set 0 and the placement's set; buildings'
  pools filed for swimming; the box sized to a building no place has;
  a dropped tile's copies taken away and the other tiles' placed
  again), into the mover's solids (028) and water (027), with no
  drawing. 043's floor and wall rule moved there too
  (`model_solids_ground`, `model_solids_wall`), her body's 0.4 by 2.3
  yards into 087 (`WORLD_HER_RADIUS`, `WORLD_HER_HEIGHT`), so the
  window and the headless run ask the same question (decision 112).
- **The headless run (089):** each tile within one of her has its
  models placed as its water is filed, from an origin at her start
  (043's); the mover asks the map's ground and the models' floors, the
  models' walls and the water. The receipt counts the models: models,
  models_solid, models_wc3, buildings, model_stand_ins,
  models_left_out, model_copies, solid_copies, solid_triangles,
  building_pools, steps_on_models, steps_walled (decision 113).
- **Moves name their map (089):** an optional `map` (the map's folder
  name); a run on another map refuses them, its receipt saying `error`
  (decision 114).
- **Two routes (092, 093):** `tests/092-test-map-moves-plaguelands.lua`
  and `tests/093-test-map-moves-master-crafters.lua`, each from its
  map's start to its water, a swim and a dive, then off the edge of its
  ground; found by a search in the mover's own steps (decision 115).

Checked (`tests/044-test-client.c`, "the placed models' floors and
walls, with no window", with fixtures made there): a game model's 12
collision triangles, none for the box or a WC3 model; a building part's
solid triangle, appended; the box's size for a building; a made tile
(a platform, the box, a WC3 model, a hut furnished in sets 0 and 1, a
building no place has) placed: 3 models, 2 buildings, 2 stood in, 7
copies, 4 solid, 37 triangles; the floor on the platform (11.5) and on
the hut's set-1 furnishing, none beside them; a wall at the platform's
side, none short of it; a placement on two tiles placed once, kept when
its first tile is dropped, gone with both. The moves' `map` read and two
bad ones refused. With the real builds and no install:

- **War in the Plaguelands:** the project's moves place 147 models
  11034 times (118 WC3 models, 28 baseline stand-ins, 1 left out: its
  `glow.mdx` is cut short), no solid shape among them, so the run is
  as in step 1n (125.7 yards). Its route: 1215 steps, 409.0 yards, 21
  swimming, 61 with no ground under her, ending at (542.04, -79.75,
  -77.19); run twice, the same receipt.
- **Master Crafters:** 11 models placed 2940 times, all the baseline
  set's stand-ins, no solid shape. Its route: 2295 steps, 796.8 yards,
  205 swimming, 61 with no ground, ending at (-541.67, -136.65,
  -87.03); run twice, the same receipt.
- Each map's run of the other's route is refused, naming the map, with
  `error` in its receipt.

Not checked: a real game model's or building's floor and wall in the
headless run (with no install both builds' models have none: WC3
models and stand-ins are walked through, decision 116; 044's made
platform and hut check the path), 043 and 042 running with 091 (they
compile under gcc and clang -Werror; raylib isn't here), and a
building's pool reached by swimming (neither build places a building).


**Step 1p is built** (2026-11-05, the same background work): a
converted map's WC3 pathing is walls, in the window and with no window,
and moves can face a direction outright. Decisions 118 to 127 (118 is
the owner's; 121 amends 117 for Master Crafters).

- **The pathing map (094):** `src/094-wc3-pathing.c` reads
  `war3map.wpm` from the build folder through the resolver (the
  converter copies the map's own, unchanged, since world-edit-to-execute's
  draft PR #252), with `war3map.w3e` for where its grid lies: "MP3W",
  version 0, cells across and up, a byte a cell, rows from the south,
  the grid's south-west corner the ground's, four cells a tile. The
  header and the bits are world-edit-to-execute's (its
  `src/demo/wc3map/footprint.lua`'s `parse_wpm`, and `new_map.lua` and
  `save.lua`, which write it): 0x02 no walking, 0x04 no flying, 0x08 no
  building, 0x20 blight, 0x40 land. A place in the converted world is
  turned back into WC3's units by map.lua's rule (wow_x = x0 + scale
  wc3_y, wow_y = y0 - scale wc3_x) to find its cell (decisions 119,
  120, 122).
- **Walls (091, 043, 089):** a cell closed to walkers that is land
  blocks a step into it; `model_solids_wall` asks the models first and
  the pathing after, and says which (1 a model, 2 the pathing), so 043's
  window and 089's run share the rule. A step is held when the place
  she steps to or her front there (her body's 0.4 yards ahead) is in
  such a cell, and she is in an open one; the wall faces along the
  cell's side she crosses, or back along the step at a corner
  (decisions 119, 126). No `war3map.wpm` means no pathing walls, said
  in the log, not an error (decision 122).
- **The receipt (089, 068):** pathing (1 when the map's was read),
  pathing_cells, pathing_closed, steps_walled_pathing (steps held by
  it, apart from the models' steps_walled), steps_on_closed (steps
  that end in a closed cell: 0 unless the rule leaks) (decision 123).
  The receipt holds 96 counts now (decision 125).
- **turn_to (089):** a move may say `turn_to`, a facing in radians (the
  game's: 0 north, rising west, from -2 pi to 2 pi); she faces it at
  once, then holds the move's keys; alone, it takes no steps. Format
  stays 1 (decision 124).
- **Routes (092, 093, 095, 096):** the two routes found again with the
  pathing as walls, as turns to a facing and runs (decision 121); and
  `tests/095-test-map-walls-plaguelands.lua` and
  `tests/096-test-map-walls-master-crafters.lua`, the same on each map:
  from her start, four seconds out and four back along each of eight
  facings, then a run north and a strafe, into the pathing's walls.

Checked (`tests/044-test-client.c`, "a converted map's WC3 pathing, as
walls", with bytes and folders made there): a 16 by 12 pathing map on
the made ground read, two cells blocking (the border's 0xCE and a bare
0x42; deep water's 0x0A doesn't); another tag, version, cells cut
short, no width, a header cut short and a scale of 0 refused; a place's
cell, and past the grid none; walls across a column and a row facing
her, at a corner facing back, held by her front 0.48 yards short with a
body 0.6 round and not 0.4; none out of a closed cell, into deep water,
past the grid or with no pathing; 091 answering the pathing's wall as
2; through the resolver, opened beside a `war3map.w3e`, not found with
no `war3map.wpm` or no `war3map.w3e`, corrupt when the grid isn't the
ground's. turn_to read, alone taking no steps, and four bad ones
refused. With the real builds (each map's `war3map.wpm` copied in by
hand, as PR #252 will) and no install:

- **War in the Plaguelands:** 1024 by 768 cells, 197472 closed land.
  The project's moves (090): 122.8 yards, 56 steps held by the
  pathing. Its route (092): 1173 steps, 439.8 yards, 14 swimming, 60
  with no ground under her, 181 held, ending at (541.70, -122.76,
  -89.23); twice, the same receipt. Into the walls (095): 1460 steps,
  417.1 yards, 593 held.
- **Master Crafters:** 1024 by 1024 cells, 153308 closed land. The
  project's moves: 126.6 yards, 64 held. Its route (093): 2295 steps,
  913.9 yards, 170 swimming, no step off the ground, 55 held, ending at
  the south border (-230.42, -495.13, 1.25); twice, the same receipt.
  Into the walls (096): 1460 steps, 473.6 yards, 375 held.
- No step on either map ends in a closed cell. With its `war3map.wpm`
  taken away, a build runs with no pathing walls and its routes are
  skipped, said so.

Not checked: 043's windowed run with the pathing (raylib isn't here;
043 compiles under gcc and clang -Werror and calls the same 091), the
converter's own copy (PR #252 isn't merged; the test builds have the
maps' own `war3map.wpm` copied in), the bit 0x80 (set on most closed
cells, named by no parser: not read), and a bridge or other walkable
model over a closed cell (none in either map; the pathing would hold
her there).

**The converter's own copy, checked** (2026-11-05, with step 1q): both
maps converted again with world-edit-to-execute's
`claude/wete-w02-wpm-copy` (its draft PR #252, commit df86a1365, run
from an export of that commit into the scratch folder; ai-stuff left as
it was). Each build's `war3map.wpm` is byte for byte the one copied in
by hand (War in the Plaguelands sha256 8bc94032…af9e56, Master
Crafters 58b55404…1c8163a, both the maps' own), as are its
`war3map.w3e`, `war3map.w3i` and `map.lua`. 044 on the two fresh builds
passes as on the hand-made ones (the same route and wall counts), but
for one check the newer converter's Master Crafters met: it brings a
second WC3 creature model, `WaterMagic.mdx`, an effect with particle
emitters alone (no geometry: 061 has nothing to draw until 403), so
that check now asks some, not every, WC3 look to have something to
draw (decision 133).

**Step 1q is built** (2026-11-05, the same background work): a search
across a table's copies, and a placeholder row where a creature's
display row is missing. Decisions 128 to 133 (128 lifts decision 98's
limit).

- **A search's rows (074):** `tables_scan` gives a table as one list
  for a reader that searches by something other than the number: the
  first place's rows in their order, then each later place's rows whose
  numbers no earlier place has. A converted map's rows are met first
  and win a search; the archives' others are still found (decision
  128).
- **Her character's tables (025):** CharStartOutfit, ItemDisplayInfo,
  CharHairGeosets, CharacterFacialHairStyles, CharSections and
  HelmetGeosetVisData are searched through it, no longer the first
  place's copy whole; where a search kept its last match (hair styles,
  skin sections) it keeps it within the first place that has one. The
  copies must agree on their width, as for a row asked by number
  (decision 129).
- **Placeholder rows (074, 060):** `tables_row_or_placeholder` answers
  a row no copy has, in a run with a resolver, with a row made in
  memory: its number in its first field, every other field 0 (text
  ""), as wide as the table's first copy; made once a row, kept until
  the tables are let go, said by the resolver as a warning and counted
  with its placeholders (`asset_row_stand_in`); strict mode refuses it,
  an error, the exit status 1 (decisions 130, 131).
- **Creatures (088, 089, 043):** a display row, or its model's row,
  that no place has: a placeholder row, and the baseline set's stand-in
  figure stands where the creature does, said. The receipt counts
  looks_placeholder (also counted in looks_stand_in) and
  table_placeholders; the windowed run's receipt carries
  table_placeholders too (decision 132).

Checked (`tests/044-test-client.c`): on the made archive and folder, a
search's rows in order (the map's 900 and its own 1, then the
archives' 0 and 530), kept, and refused or not found as a lookup is; a
placeholder row's number, zeros, empty text and width, made once,
warned and counted once, the report naming it; a row a copy has given
as itself; no placeholder for a plain lookup, a table no place has, or
with no resolver; strict mode refusing it with the exit status 1. Her
tables across a map's folder and another place: the map's outfit and
look found, the other place's outfit still found, a look both have the
map's, a character built from both. A creature display no place has,
and one whose model row no place has: the stand-in figure, placeholder
1. On each real build, its creatures and one more naming display
3999999: the run ends closed, that look stood in by a placeholder row,
looks_placeholder 1 and table_placeholders 1 in the receipt.

Not checked: the windowed run's creature built from a placeholder row
(raylib isn't here; 041 builds the stand-in 088 gives, as for a model
no place has), and the light tables (026), still read whole from the
first place (decision 129).

**Step 1r is built** (2026-11-05, the same background work): the
tables' layouts (issue 106), `src/097-table-layouts.c`. Decisions 134
to 139.

- **What each column is (097):** for the twenty tables this client
  reads or the converter writes, each column's name, kind (whole
  number, signed, decimal, text, or a localised text as 16 texts and a
  mask), its placeholder value, and the readers that read it. The
  converter's seven (Map, AreaTable, LoadingScreens,
  CreatureDisplayInfo, CreatureModelData, FactionTemplate, Faction)
  are world-edit-to-execute's `src/wowbuild/dbc.lua` LAYOUTS, all 221
  columns' names and kinds checked equal to it, one by one; the
  others' read columns are the readers' own offsets (025, 026, 040,
  041, 087, 088), the rest recalled. A column a reader reads is marked
  with that reader (checked from code); every other column, and every
  width, is unchecked: neither project has a 3.3.5a file to measure
  (decision 134).
- **Widths (074, 026):** a copy narrower than the last column this
  client reads is refused, an error naming its place, its width and
  that column; a copy of another width than its layout's is said on
  the standard error and read as it is, since the layout may be the
  wrong one (decision 135). 026's light tables are checked the same
  way (Light now to field 9, which it reads, and LightParams and
  LightSkybox too).
- **Placeholder rows from the layout (074):** a placeholder row's
  columns hold the layout's values: a size of 1, an opaque alpha, water
  three-quarters opaque, a visible name "unknown Map 900" in its
  first-language text with the mask, the rest 0 or "" (decision 136).
  A table no place has can now take one too, as wide as its layout; a
  table with no layout still gets the old row of its number and zeros,
  and one whose first column isn't a row number
  (CharacterFacialHairStyles) gets none (decision 139).
- **Who asks (041, 088):** 088 as before; 041's window now asks for its
  CreatureDisplayInfo and CreatureDisplayInfoExtra rows the same way,
  and a placeholder look row stands the stand-in figure in, said
  (decision 137). Map, LiquidType and the rest stay an error.
- **The baseline set (085):** its tables' widths come from the layouts
  (decision 138); its bytes are as before.

Checked (`tests/044-test-client.c`): every layout's columns fill its
width exactly and are named once; the readers' last columns (Map 2,
CreatureDisplayInfo 9, LiquidType 16, LightParams 9,
CreatureDisplayInfoExtra 21); columns by number, a localised text's
names and mask; a too-narrow width refused naming both, a wider one
fitting; placeholder rows for CreatureDisplayInfo (its number, size 1,
alpha 255), Map (its "unknown Map 77", its mask, a wider copy's extra
columns 0) and LightParams (0.75); on the made archive and folder, a
3-wide CreatureDisplayInfo refused, a Faction no place has given a
57-wide placeholder row named "unknown Faction 3", a table with no
layout and CharacterFacialHairStyles given none; each real build's
seven tables as wide as their layouts.

Not checked: any width or unread column against a real 3.3.5a file
(none in either project); the windowed run's look from a placeholder
CreatureDisplayInfoExtra row (raylib isn't here; 041 compiles under
gcc and clang -Werror); the mask 0xFF01FE (the converter's, recalled).

**Step 1s is built** (2026-11-05, the same background work): a map's
tile list from its WDT, `src/098-wdt.c`, in the reader library.
Decisions 140 to 144.

- **The reader (098):** `wdt_read` takes a WDT file's bytes and gives
  its version (MVER), flags (MPHD), each of the 64 x 64 tiles' flags
  (MAIN) and how many have ground, and for a map that is one building
  its name (MWMO) and placement (MODF, read as an ADT's building by
  020's `adt_place_read`, now shared); `wdt_has_tile`,
  `wdt_big_alpha`, `wdt_wmo_only` ask it. The struct holds no
  pointers, so the editor's LuaJIT FFI can use it as it uses the ADT
  reader (decision 140). What neither project writes or reads (MPHD's
  0x1, MAIN's other bits, a building map's MWMO and MODF) is marked
  unchecked in 098-wdt.h.
- **The client asks the list, not every file:** the ground (033, so
  the window's tiles and 034's reading ahead), the heights (075, so
  the start and the mover's ground) and the run with no window (089)
  take a tile the WDT doesn't list as open sea without asking for its
  file; a tile it lists that no place has is open sea too, said
  (decision 141). 043 says how many tiles the WDT lists, and names a
  one-building map's building, not yet placed (decision 144).
- **Agreement with the converter (decision 143):** world-edit-to-
  execute's W02a writer, from its commit df86a1365 (draft PR #252;
  exported into the scratch folder, ai-stuff left as it was), called
  directly for three tiles that read differently turned round, its
  bytes read by this `libwreaders.so` through LuaJIT's FFI and the
  `wreaders.h` scripts/build writes: each tile listed where its file
  <dir>_<tx>_<ty>.adt is read, none turned round. And five maps
  converted by the same commit (War in the Plaguelands, Master
  Crafters, DAoW-2.1, Conquest-v8-fix: 2 x 2 tiles each; DaoW (HvA)
  7.5: 4 x 4 from 30_30): each WDT lists exactly the tiles whose ADT
  files the build holds, 4096 of 4096 agreeing.

Checked (`tests/044-test-client.c`): made WDTs (three tiles listed by
tile x * 64 + tile y and not turned round; a one-building map's name,
placement in world axes and furnishing set; refusals naming a missing
MVER, a short MAIN, a block past the end, no MPHD, no MAIN, a short
MODF); on each converter build, its WDT's version, flags and tiles
against the files, the ground (033) and the heights (075) taking an
unlisted tile as open sea before a file is asked for. With the
original's archives (not here): Kalimdor's tile list with Astranaar's
tile, and every map in Map.dbc read, its building maps naming a .wmo
and its placement.

Not checked: any real 3.3.5a WDT (no install here; the checks above
run on the owner's machine); a one-building map's MWMO and MODF and
MPHD's 0x1 (recalled, as in 303); the windowed run's tile count line
(raylib isn't here; 043 compiles under gcc and clang -Werror).

**Step 1t is built** (2026-11-05, the same background work): the sky
and fog from the light tables, read through the client's tables.
Decisions 145 to 150.

- **Reading (026 on 074):** Light, LightParams, LightIntBand,
  LightFloatBand and LightSkybox are read through 074 like every other
  table: one list of rows across every place (the map's own first),
  each copy's width checked against its 097 layout, and a placeholder
  row where a band or parameter row is in no place (the resolver says
  it; a band with no moments takes the grey day's value for its slot). 026's own file reading, row search and width check are
  gone (decision 145).
- **Picking (026):** a map's default light (position 0, 0, 0) is laid
  first, then each of its lights that reaches the point, farthest first
  by distance from its centre (two within a third of a yard: the larger
  inner radius first), each full inside its inner radius and fading in
  a straight line to its outer; the nearest is laid last, on top
  (decision 146). A map with no default of its own takes Light's row 1;
  with no row 1, the grey day, said once (decision 147). The colour
  bands and fog distances are blended in straight lines between their
  moments by the time of day, across midnight, as before.
- **Missing tables:** when any of the four is in no place, the fixed grey
  sky and fog (fog 125 to 500 yards, water 0.75) stand everywhere, said
  in one line naming them; LightSkybox in no place means no skybox,
  said (decisions 145, 150). A copy that is there but broken is an
  error, named, as before.
- **Drawing (043):** the window's clear colour is the light's fog colour
  and its fog distances and sky colours go to the shaders, as before,
  now from 074's rows. Its "no lights" line is not said when the tables
  aren't there (the one line above says it).
- **With no window (089):** the run takes the light where she ends, at
  noon unless 043 is given `--hour` (decision 148), with a test map's own
  war3map.w3i fog laid over it and held to the far clip as the window
  does (084), logs it, and counts it in the receipt: `light`,
  `light_tables`, `light_hours`, `fog_from_map`, `fog_start`, `fog_end`,
  `fog_red/green/blue`, `sky_red/green/blue`.

Checked (`tests/044-test-client.c`): made Light, LightIntBand,
LightFloatBand, LightParams and LightSkybox tables in two places with
known values: seven lights merged, the map's first; away from every
sphere at noon the map's default (not the later place's), its red fog
from 500 to 1000 yards, blue sky, water alphas and skybox; the fog's
colour at midnight, 6:00 and 18:00 (half way each side); inside a
sphere's inner radius the sphere whole; half way between its radii half
of each; two spheres both holding a point, the nearer laid on top though
it comes a row earlier (laying them in row order fails the check); a map
with no lights, and one whose only sphere doesn't reach, taking row 1;
inside that map's sphere, the sphere over row 1; an underwater set no
place has: every band a placeholder, the grey day, the resolver naming
LightIntBand; Light.dbc alone: the grey day and one line naming the
other three; no table at all: the same. On the converter builds (whose
sets hold the four tables empty): no light and no row 1, said, so the
grey day; the run with no window at noon on each, twice the same, its
fog War in the Plaguelands' own (113.9 to 162.8 yards, its colour) and
Master Crafters' the grey day's (125 to 500; its w3i turns no fog on).
With the original's archives (not here): the install's lights read
through 074.

Not checked: the farthest-first order, its third-of-a-yard tie and
which inner radius goes first in a tie, and the row-1 fallback against
the original (from 306's clean-room notes, not run); any real 3.3.5a
light table (none here); the window's sky and fog (raylib isn't here;
043 compiles under gcc and clang -Werror).

**Step 1u is built** (2026-11-05, the same background work): the
original's light rules from 306's notes, in `src/026-light.c`.
Decisions 151 to 156.

- **The test program's output (044):** its standard output is now line
  buffered, so a warning a part writes to the standard error lands
  between check lines, not inside one, and `grep -c '^  ok:'` counts
  every check again (decision 156).
- **Colours in whole 255ths (026):** each colour band's value, after the
  blend between its two moments, is cut down to a whole 255th, as the
  original stores it; lights and storms are blended after that, unrounded
  (decision 151).
- **The fog's rules (026):** on the blended light, the start's share of
  the end is held to -1 to 1 and the end is no nearer than 10 yards; the
  far clip still ends it, as before (084, 173b6). Below map 530 a
  negative share is kept (the fog starts behind the eye); from map 530
  on, the second fog mode, it counts as 0 (decisions 152, 153). The
  light says which mode its fog follows (`fog_mode`: 1, 2, or 0 for the
  grey day and a test map's own fog); 043 prints it and the run with no
  window counts it in the receipt. The second mode's curve, its rate and
  its end pushed to the far clip are not built (decision 153).
- **Storms (026):** `light_at_weather` lays each light's storm set
  (Light's field 10, under water 11) over its own by the storm's strength,
  0 to 1; a light naming none keeps its own. Nothing reads the weather
  yet, so 043 and the run with no window pass 0 (decision 154).
- **Left, with reasons (decision 155):** zone lights, override lights,
  the indoor fog blend, and the second fog mode's curve.

Checked (`tests/044-test-client.c`): made tables: the fog's colour at
1:00 (21.25 cut to 21) and 6:00 and 18:00 (127.5 cut to 127); a fog end
stored as 180 inches is 10 yards, not 5, with its share of -0.5 kept on
map 1 (fog from -5) and counted as 0 on map 571 (from 0), and a share
of -3 held to -1; a storm of 0 the same as no storm, half laying half
the storm set (fog colour, distances; a band the storm set lacks the
grey day's), whole laying it all; a light with no storm set keeping its
own in a whole storm, half of it and half the stormy default between its
radii. On the converter builds: the run with no window's fog mode 0
(their sets' tables are empty: the grey day, or the map's own fog).

Not checked: every rule here against the original (306's clean-room
notes, not run): the truncation (rather than rounding to the nearest),
which values it applies to, and whether lights are blended before or
after it; the 10-yard floor and the share's range applied once to the
blended light rather than to each; map 530 as the second mode's first
map; the storm sets' field numbers (026's layout, recalled).

**Step 1v is built** (2026-11-05, the same background work): the
weather for the storm sets, and a building's fogs. Decisions 157 to
165; the new file `src/099-weather.c`.

- **The weather message (099):** `weather_msg_read` reads SMSG_WEATHER's
  body as 9 bytes (state, grade, abrupt), refusing another length, a
  grade that isn't a number or an abrupt byte other than 0 or 1, naming
  it; the whole layout is recalled, not read from code (this client has
  no network code, its docs don't list the message, and no server source
  is here), and marked so (decision 157).
- **The storm's strength (099):** every state but fine lays the storm
  sets over by its grade; a change eases over ten seconds in a straight
  line from wherever the strength stands, or is there at once when
  abrupt (decision 158).
- **Where the weather comes from, for now (043, 089):** `--weather STATE
  GRADE` from the start, at once, for the window (eased on the world's
  seconds each frame into `light_at_weather`) and the run with no
  window; and in a moves file, a move's `weather = { state, grade,
  abrupt }`, taken as the move begins on the steps' clock (a weather
  alone takes no steps; format 1 still, older files read as before).
  The receipt counts `weather_state`, `weather_grade`,
  `weather_changes` and `storm`, the strength where she ends
  (decision 159).
- **A building's fogs (021):** the root's MFOG fogs (a sphere with two
  radii, a fog in and under water) and each group's four fog numbers and
  bounding box are read, recalled and unchecked; `wmo_group_depth` says
  how far a point is inside a group's box (decision 161).
- **The indoor blend (026):** `light_indoor_fog` blends the outdoor fog
  into a group's own over the first 25 yards in, its underwater fog
  under water, as 306's notes describe (decision 162). Which of a
  group's fogs applies, and whether the eye is in a group, are not in
  the notes: nothing calls it yet but the tests (decision 163).
- **A converted map's own light (world-edit-to-execute's draft PR
  #260):** a build converted with it carries Light and its tables in
  the map's folder, the map's own default light first. The checks on a
  build take either: the grey day where the build has no light rows,
  its own default's id where it has (decision 165). Saying "no row 1"
  moved from opening the tables to `light_map_default`, called once for
  the map being lit, so a map with its own default isn't told about
  row 1 (decision 164).

Checked (`tests/044-test-client.c`): a made weather body read (state,
grade, eased); refused, named: 8 bytes, 13 (an older client's, with a
sound id), a grade that isn't a number, an abrupt byte of 2; a grade of
1.5 held to 1; fine weather is no storm whatever its grade; the easing
(0, 0.3, 0.6 at 0, 5 and 10 seconds; fine at once; a change half way
through eased from where it stood: 0.5, 0.25, 0); moves with weather
read, and five broken ones refused by name; the run with no window on
each converter build: light rain from the start and heavy rain eased in
two seconds on, half way where she ends (0.5), its state, grade and
two changes in the receipt; with no weather, fine and none taken. A
made building: two fogs read field by field, a group's fog numbers and
box, how far in a point is (the middle, by a wall, outside, past a
corner), the blend outside (none), 12.5 yards in (half), 30 in (the
group's), under water (its underwater fog). The light under every
sphere (map 900 its own default, 902 row 1, 571 its own), nothing said;
the set's empty tables, with no row 1, said for the map. On the two maps
converted by #260's commit 306b40f40 (exported to the scratch folder;
ai-stuff left as it was): the map's own light 20000 from its own rows,
the map's folder's copy first, no placeholder row made and no word of
row 1; the run with no window lit by it, twice the same; with no
baseline set, still lit by it, saying only that no skybox is drawn
(#260 writes no LightSkybox).

Not checked: the weather message's layout, opcode and state numbers,
and that 3.3.5a sends no sound id (recalled); how the original turns a
state and grade into a storm's strength, and how long it eases; the
building fog's layout and units; the blend's straight line and the box
as the group's boundary; the window's weather (raylib isn't here; 043
compiles under gcc and clang -Werror).

**Step 1w is built** (2026-11-05, the same background work): the
tables' layouts held to the converter's. Decisions 166 to 171; the new
check `tests/100-layouts-match-converter.lua`.

- **The check (100):** given a world-edit-to-execute tree (`--wete DIR`;
  its `src/` is enough, so an exported commit works), it has the
  converter's own `src/wowbuild/dbc.lua` write one row of every table
  in its LAYOUTS, every column a different value (whole numbers from
  the column's place, signed ones below zero, decimals, texts naming
  the column), then reads the written bytes back through this client's
  table reader (016) by 097's layout and compares, column by column:
  the fields and bytes a row, the name (a localised text's columns by
  their base name), the kind, the value read back, the localised mask,
  and that every column the converter's `seen` says this client reads
  is one 097 marks as read. Columns 097 reads that `seen` doesn't list
  are printed as notes. With no tree it says so and passes (decision
  166).
- **097 in the reader library:** the check asks the layouts from Lua,
  so 097 moved from CORE's own list to READERS (CORE still has it,
  through READERS; it needs only 013) (decision 167).
- **One disagreement, on this side:** the converter's Light (#260)
  says this client reads its fields 8 to 12; 097 marked only 8 and 9
  read, though 026 reads all five sets since step 1u (the storm sets
  10 and 11, and 12, read and not yet used). 097 now marks 10 to 12
  read, so a Light copy narrower than 12 fields is refused (decision
  168).
- **The light tables' widths now name the converter:** Light 15,
  LightParams 9, LightIntBand and LightFloatBand 34 are the baseline
  set's and #260's alike.
- **044:** a build's width check takes the four light tables as well,
  when the build has them (one converted with #260), and passes them
  over when it doesn't.

Checked: against world-edit-to-execute's commit 306b40f40 (#260, its
`src/` exported to the scratch folder; ai-stuff left as it was), all
eleven tables the converter writes agree: AreaTable (36), CreatureDisplayInfo
(16), CreatureModelData (28), Faction (57), FactionTemplate (14), Light
(15), LightFloatBand (34), LightIntBand (34), LightParams (9),
LoadingScreens (4) and Map (66). Planted disagreements were caught and
taken back: LightParams' Glow as a whole number (its kind and its value
read back), Map's ExpansionID and RaidOffset swapped (both names and
values), and Light's sets 3 to 5 unmarked (the converter's `seen`).
On the two #260 builds, 4 of 4 light tables are as wide as 097's.

Not checked: either copy against a 3.3.5a file (still none in either
project); the nine tables of 097 the converter doesn't write
(CreatureDisplayInfoExtra, CharStartOutfit, ItemDisplayInfo,
CharHairGeosets, CharacterFacialHairStyles, CharSections,
HelmetGeosetVisData, LiquidType, LightSkybox), whose widths come from
the readers, the test program's made tables and the baseline set
(decision 169).

**Step 1x is built** (2026-11-05, the same background work): the six
failures every run here carried, looked at one by one. Decisions 172 to
176; no new file.

The six, as a run on this machine printed them, and what became of each:

1. **"Astranaar's flight point stands at -100000.00 yards (Lua:
   106.71)"**: the ground's height at a known place, from the server's
   extracted map files (`PROJECT_DIR/assets/server-data/maps`). That
   folder isn't on this machine, and -100000 is 018's "no ground here".
   Not a bug: now a NEEDS naming the missing folder.
2. **"the server's tiles for Kalimdor say -1.0 degrees is the steepest
   walkable"**: the steepest walkable slope, from the server's
   navigation tiles (`assets/server-data/mmaps`). Not on this machine:
   019 says it can't open the folder and gives -1. Now a NEEDS; with the
   folder there, a wrong value still fails, now with 019's sentence.
3. **"up a 45-degree ramp: walks on"** and
4. **"on a 45-degree slope, standing still stays put"**: the mover on
   made-up slopes, held to the limit from 2. A fault in the test: they
   took 019's -1 as the limit, which makes any slope over a degree too
   steep. So these two failed, and the two 70-degree checks beside them
   passed without the limit being tested. Without the tiles they now
   take 60, the value 2 holds the server to, and all four pass on
   merit; with the tiles, the server's value as before (decision 174).
5. **"a second's run from the start point on Ashenvale's real ground
   stays on it"**: the mover on the server's real ground, needing the
   map files of 1. Now a NEEDS.
6. **"no MPQ archives found in x"**: the original client's install
   (`CLIENT_DATA_DIR`), not on this machine. This was also a fault in
   the test: the program printed a FAIL without counting it and
   returned. Every check after the archives (sight, creatures, errors,
   settings, collision, swimming, the camera, as well as those that read
   the install) never ran here, and no closing line was printed. Now
   each part that reads the install is one NEEDS, and the rest run
   (decision 173).

Also, by the same rule: the seam check beside 1 passed with nothing
compared when the map files were absent. It is now a NEEDS then, and with
them it must compare some points (decision 175). WC3's war3.mpq,
reached for the first time here once the run went on, is a NEEDS when
absent (decision 176).

Checked (`tests/044-test-client.c`), on every build here:
- 0 failures and 12 NEEDS: the four server-file checks, war3.mpq, and
  the install's seven parts.
- 83 more passing than before: the 82 after the archives that need no
  data, now made here (WC3's made picture, sight, creatures, errors,
  settings, collision, swimming, the camera), and the two 45-degree
  slopes, less the seam check, now a NEEDS.
- Exit status 2 throughout.
- The sanitized runs are clean. The leak report is now empty: its 35184
  bytes (2 allocations) were the ground world (33344) and the settings
  store (1840) that the early return left. With the run reaching the end,
  only the last settings store was left, and it is now freed.

No expectation was found wrong and no fault was found in the client.
None of the six measured anything here.

Not checked: the values these checks hold the files to (106.71 yards,
60 degrees, the install's and war3.mpq's facts) need the owner's
machine. A run there should show 0 NEEDS. If it shows any, that file
has moved. Where the install is here but the server's files aren't,
the reading-ahead and ground-file parts still treat the missing server
folder as a failure; no machine here has the install, so that is left.

**Step 1y is built** (2026-11-05, the same background work): a map's
loading screen picture, which the converter writes and the client had
ignored. Decisions 177 to 182; the new file `src/101-loading-screen.c`
and the check `tests/102-loading-screen-matches-converter.lua`.

- **Finding it (101):** the map's Map row names a LoadingScreens row
  (LoadingScreenID), that row names the picture (FileName), and the
  picture is read from the first place that has it (a test map's
  folder first) and decoded by 017. Columns are found by name in 097
  (`table_layout_find`), not counted (decision 178). Each step that
  finds nothing says which and which number. No picture is no loading
  screen, never the checker (decision 179). HasWideScreen is read and
  kept, not used (decision 181).
- **A narrow Map copy:** LoadingScreenID is field 58. 097 leaves it
  unmarked, so 074 still takes a narrower Map copy for everything else;
  101 says the copy is too narrow and gives no loading screen (decision
  180).
- **The window (043):** right after the window opens, before the world
  loads, the picture is drawn once, stretched across the window (the
  converter's account of the original, unchecked). Then the load
  proceeds as before. Compiled under gcc and clang; not run (no raylib
  here).
- **The run with no window (089):** finds it as the window does, says
  it, and its receipt counts `loading_screen` (the row),
  `loading_screen_width`, `_height` and `_levels`.

Checked (`tests/044-test-client.c`):
- On tables and pictures made here: columns by name (58, 2, 3, 4).
- Map 901's screen from a made 4 x 4 picture with three copies, as made.
- Six refusals, each named, and no placeholder stood in for any of
  them: none named, no row, no file named, no picture anywhere, a file
  that isn't a picture, no Map row.
- A 3-field Map copy: no loading screen, while its row is still taken.
- On every converter build here (#260's among them): the Map row
  names the LoadingScreens row map.lua names (900), the folder's
  row, its picture `World\Maps\<dir>\<dir>_loading.blp` from the
  folder, 512 x 512 with 10 copies, HasWideScreen 0. The first copy is
  the picture, and each smaller copy is exactly the one before halved,
  as the converter's loadscreen.lua halves it.
- The run with no window reports the same in its receipt.

`tests/102-loading-screen-matches-converter.lua` holds the reader to the
converter's writer (decision 182). The converter's own `loadscreen.blp2`
writes four made pictures: its 512 size; a 300 x 200 with see-through
parts at 64; a 3 x 5 grown to 16; a 1 x 1. 017 reads each copy back
pixel for pixel equal to the converter's own resample, with no copy past
the last. Without a converter tree it is skipped, said, exit 0. A
planted fault (017 not swapping red and blue) failed all copies and
was taken back.

Not checked: whether the original shows a BLP2 of encoding 3 (the
converter marks it unchecked) or stretches the picture; what
HasWideScreen does there (recalled: a wide copy); the converter's first
copy against its source (the map's preview picture or its ground,
not in the build). The rest of 606 (the bar, its rounds, the tips, input
swallowed, ending when the reads finish) is not built.

Not built yet: a WC3
model posed and animated (it stands in its rest pose; 402 step 4, PR
#7), with its particles (403) and solid shape (decision 116), or team-coloured (404:
its replaceable pictures are left off); the baseline set's buildings, creatures' own looks, her character, WC3
stock art and the rest of the client's tables (decisions 88, 97), and
the asset forge's real set;
a placeholder row for tables other than a creature's (step 1r gives
every table the client reads a layout, but only 041 and 088 ask for
one); the layouts written out for the converter (decision 134); the
light's other differences from the original (306: the second fog
mode's curve and rate, zone and override lights, which of a building
group's fogs applies and whether the eye is in the group, so the indoor
blend can be used, the seven-ring sky dome; decisions 155, 163), and
the weather from the server itself (decision 159; Phase 5's network
code); logging in, and the `lost-connection` ending
(Phase 5's 609).

## Intended behavior

The editor has a **Test Map** button (F5, as world-edit.exe's). It saves
the map being edited, converts it (world-edit-to-execute's W02: ground,
placements, the map's own imported files, its rules as server scripts),
starts a local AzerothCore with it if one isn't running, and opens this
client on that map. The player is in the map as players will be: rules
kept by the server, drawing done here, the asset lookup (105a) in its
usual order. Closing the client returns to the editor.

The editor itself stays in world-edit-to-execute and keeps that
project's Lua renderer for editing. The owner (2026-11-04): "It has a
lua renderer right now, but we're building a C renderer for in-game -
the lua renderer is only for the editor, not the client." And: "there
should be a button to test the map that opens the w-client. The map
editor should have a renderer (lua) that displays the map and such so
you can edit it of course." So two renderers, each with one audience:
the editor's draws what is being edited, this one draws what is being
played. Neither retires the other.

What this client needs for Test Map:

- **A map handed in from outside**: a command-line option naming the
  converted map's folder (an overlay for 105a, step 2) and where to
  start the camera, so the editor can launch it without menus.
- **WC3 maps drawn**: the ground painted onto the one ground mesh with
  WC3's tilesets and cliffs seated on it (docs/009, answer 1); doodads
  and units with their models (401 step 2, MDX); the flat-shaded designs
  as placeholders (docs/009, answer 2); wire-frames where the player has
  no model (105a, step 4).
- **The RTS camera, picking, selection, orders** (1101-1104), shared
  with WC3 map mode: Test Map is WC3 map mode, opened by the editor.
- **A clean ending**: a goodbye receipt the editor reads to tell the
  map ran (and how long, how many frames), and a crash reported back as
  a missing receipt.

## The launch option and the receipt

The contract with world-edit-to-execute's editor (its issue 913, built
there at the same time), written 2026-11-04. Either side changes it
only with the other.

```
w-client --test-map <folder> --receipt <file> [--camera <x>,<y>] [--realm <host>:<port>] [--account <name>]
```

- `--test-map <folder>`: the converted map's build folder. For this run
  only it is put in front of the overlays the settings name
  (`assetOverlays`); the settings file isn't changed. 105a's order
  stands around it: the player's packs, then a server's delivered
  assets, then this folder, then the settings' overlays, then the
  player's own archives, then a placeholder.
- `--receipt <file>`: where the goodbye receipt goes. Its folder must
  exist when the client starts.
- `--camera <x>,<y>`: world x and y in yards, a comma between, no
  spaces (`12.5,-300`). Without it, the map's own start.
- `--realm <host>:<port>`: the local AzerothCore's login server;
  `127.0.0.1:3724` when not given.
- `--account <name>`: the account to log in with, 1 to 63 characters,
  no spaces.
- **The password** comes from the environment variable
  `W_CLIENT_PASSWORD`, never the command line (every user of the machine
  can read a command line). A test-map run without it, or with it empty,
  stops before the window opens, with a receipt saying why. The client
  takes it out of its environment once read.
- **The client's own options** come too: `--maps DIR` (the server's map
  files) and `--client DIR` (the player's game Data folder) are needed
  by every run. `scripts/run` adds them, with `--settings`, `--world`
  and `--assets`, and passes the rest on as given, so the editor's
  setting for "where the W client is" (913 step 1) names
  `scripts/run`; one naming the built `w-client` must add the two.
- A value is missing when the arguments end or the next one starts with
  `--`. Any option the client doesn't know, a missing value, a bad
  value, an option given twice, or `--receipt`, `--camera`, `--realm`,
  `--account` without `--test-map` stops the start; with a receipt
  named, the receipt says which.

The receipt is a Lua table of plain data, written all at once (a
temporary file in the same folder, renamed onto the name), so the editor
never reads half a receipt. LuaJIT reads it with `loadfile(file, "t",
{})`; it holds only what the client's own data-only reader takes
(059-loose-folders.h). Format 1:

```lua
-- the W client's goodbye receipt (issue 1109): the editor's Test Map reads it back
return {
    format  = 1,
    map     = "/home/me/maps/daow-build",  -- the --test-map folder, as given
    started = "2026-11-04T20:31:07Z",      -- when the client started, UTC
    ended   = "2026-11-04T20:33:52Z",      -- when it wrote this, UTC
    seconds = 165.204,                     -- between the two, by the steady clock
    frames  = 9873,                        -- frames drawn
    reason  = "closed",                    -- "closed", "lost-connection" or "error"
    why     = "...",                       -- only with "error": the sentence
}
```

- `closed`: Escape, or the window closed. `lost-connection`: the server
  went away (nothing ends so until Phase 5's 609). `error`: the client
  stopped on something it couldn't do, said in `why`.
- **No receipt means a crash**, and the editor says so. The receipt is
  written after everything else is let go, so a crash while letting go
  counts as one too. Two other ways to end with none: a receipt folder
  that isn't there when the client starts (it says so on the terminal
  and exits with 2), and a failure in `scripts/run` before the client
  starts (no maps folder, no client, a build that fails).
- Exit status: 0 after `closed`; 2 for the options (a usage error); 1
  for any other failure of the start, or a receipt that couldn't be
  written at the end.

## Suggested implementation steps

1. ~~The launch option: open a converted map folder as an overlay, skip
   the login and character screens, place the camera.~~ Done
   (2026-11-04): 067 and 068, wired into 043, with the receipt
   ("A clean ending"). No screens exist to skip yet. (Then, the map's
   folder was in a resolver no reader asked, and the map was `--map`'s;
   step 1b changed both.)
1b. ~~Open the converted map: its map, start, tables, ground and own
   models from the build folder.~~ Done (2026-11-04): 073, 074, 075,
   the resolver's table and ground kinds, 041 reading a map's `.mdx`
   (open question 4 answered; decisions 12 to 19).
1c. ~~The map's placements: its doodads and buildings, with its own
   models and pictures, and its ground pictures, through the
   resolver.~~ Done (2026-11-05): 076, the resolver's `asset_get` and
   `asset_stand_in`, 042 and 033 asking through them (decisions 20 to
   31).
1d. ~~The player's own WC3 install as a place, and the map's ground in
   WC3's tilesets.~~ Done (2026-11-05): `assetWc3`, 015's WC3 chain,
   077, 078, PR #3's 063 merged (decisions 32 to 41).
1e. ~~The map's cliffs as WC3's own cliff models, on their ground.~~
   Done (2026-11-05): 079, 078's cliff ground and placements, 076's
   replaceable-picture mark (decisions 42 to 50).
1f. ~~Ramps as WC3's own ramp models, and blight.~~ Done (2026-11-05):
   079's ramps, 078's blight from the editor's data (decisions 51 to
   56).
1g. ~~The ground laid cell by cell: every tileset in a chunk, each
   cell's variation.~~ Done (2026-11-05): the ground's shader (080)
   reading each cell's pieces from 078 through 033 and 038 (decisions
   57 to 63).
1h. ~~Cliffs and ramps bent to the ground, and WC3's water read.~~
   Done (2026-11-05): 079's bend in the model's name, 076 bending the
   model, 081 reading WC3's water (decisions 64 to 69).
1i. ~~WC3's water drawn, tinted and reconciled (the water plan's steps
   2 to 4).~~ Done (2026-11-05): 082's points and shader, 040 drawing
   them, 083 reading `war3map.w3i`, 081 tinting (decisions 70 to 75).
1j. ~~The map's own WC3 fog drawn.~~ Done (2026-11-05): 084 reading the
   map's `war3map.w3i` fog into yards, 043 putting it in each frame's
   light, under every drawer (decisions 76 to 80).
1k. ~~The baseline set as a place, and its smallest form.~~ Done
   (2026-11-05): 060 asking it after the Blizzard installs (setting
   `assetBaseline`), 040's water through the resolver, 085 and
   `make-baseline` (086) making it from the converter's tables
   (decisions 81 to 88).
1l. ~~A test-map run with no WoW install.~~ Done (2026-11-05): 087
   the start out of the window, 085's tables and stand-in figure, 088
   and 041 finding a creature's model through the resolver, 026 and 040
   reading their tables through it (decisions 89 to 97).
1m. ~~Nothing in a test-map run past the resolver.~~ Done (2026-11-05):
   025's character and 034's read-ahead through it (decisions 98 to
   101).
1n. ~~A headless test-map run.~~ Done (2026-11-05): `--headless MOVES`
   (067, 089), the project's moves (`tests/090-test-map-moves.lua`),
   the receipt's counts (068) (decisions 102 to 110).
1o. ~~The placed models' floors and walls with no window, and routes to
   water and the edge.~~ Done (2026-11-05): 091 (shared with 042 and
   043), 089 placing them, `tests/092` and `093` (decisions 111 to
   117).
1p. ~~WC3's pathing as walls, and turning to a facing.~~ Done
   (2026-11-05): 094 reading `war3map.wpm`, 091's wall rule asking it
   for 043 and 089, `turn_to` in the moves, `tests/092`, `093`, `095`
   and `096` (decisions 118 to 127).
1q. ~~A search across a table's copies, and placeholder rows for
   creatures.~~ Done (2026-11-05): 074's `tables_scan` and
   `tables_row_or_placeholder`, 025 searching through it, 060's
   `asset_row_stand_in`, 088 standing the stand-in in, the receipt's
   counts (decisions 128 to 133); the converter's own `war3map.wpm`
   checked against the hand-copied one.
1r. ~~The tables' layouts (106).~~ Done (2026-11-05): 097, 074's
   width check and layout-filled placeholder rows, 026's width check,
   041 asking for placeholder rows, 085's widths from the layouts
   (decisions 134 to 139).
1s. ~~A map's tile list from its WDT.~~ Done (2026-11-05): 098 in the
   reader library, the ground (033, 034), heights (075) and the run with
   no window (089) asking its list, checked against the converter's own
   WDTs (decisions 140 to 144).
1t. ~~The sky and fog from the light tables.~~ Done (2026-11-05): 026
   through 074 (rows across places, placeholder rows, 097's widths),
   farthest-first laying and Light's row 1 for a map with no default,
   the grey day said when the tables are missing, and the light where
   she ends in the run with no window and its receipt (decisions 145
   to 150).
1u. ~~The original's light rules.~~ Done (2026-11-05): colours cut to
   whole 255ths, the fog end's floor and the start's range, the two fog
   modes' start rule, storm sets by strength, and the test program's
   output line buffered (decisions 151 to 156).
1v. ~~The weather and a building's fogs.~~ Done (2026-11-05): 099 reads
   the weather message and eases the storm's strength, from `--weather`
   or a moves file for now, counted in the receipt; 021 reads a
   building's fogs and 026 blends them in indoors, not yet called;
   a converted map's own light (#260) taken by the checks, and "no row
   1" said only for a map with no default (decisions 157 to 165).
1w. ~~The tables' layouts held to the converter's.~~ Done (2026-11-05):
   `tests/100-layouts-match-converter.lua` has the converter write a
   row of each of its eleven tables and reads it back by 097; all agree
   once 097 marks Light's sets 3 to 5 read; 097 is in the reader
   library (decisions 166 to 171).
1x. ~~The six failures every run carried.~~ Done (2026-11-05): all six
   needed files only the owner's machine has. They are now NEEDS,
   counted and printed apart, with exit status 2. Two of the six were
   faults in the test: slopes held to a missing limit, and a missing
   install that ended the run. Those are fixed, and 82 checks that never
   ran here now run and pass (decisions 172 to 176).
1y. ~~A map's loading screen picture.~~ Done (2026-11-05): 101 finds it
   through the map's row and LoadingScreens, and decodes it. The window
   shows it while the world loads, and the run with no window counts it
   in the receipt. Held to the converter's writer by tests/102
   (decisions 177 to 182; 606's picture only).
2. The editor's side (in world-edit-to-execute): F5 saves, converts,
   starts or reuses the local AzerothCore, launches this client with
   the option, and waits for the goodbye receipt.
3. WC3 ground on the one mesh (302), then models (after 401 step 2).
4. The RTS controls from 1101-1104.
5. Check: the same map test-played twice gives the same receipt counts
   for the same scripted input. The client's side is built (step 1n:
   a headless run twice on War in the Plaguelands and Master Crafters,
   in the test program; step 1o: with the models' floors and walls,
   and each map's route to its water and off its edge, twice; step 1p:
   with WC3's pathing as walls, War in the Plaguelands' route off its
   edge and Master Crafters' to its closed border, twice; step 1q: on
   the converter's own builds too, and with a creature whose display
   row no place has counted, not missing); the check
   through the editor's F5 waits on step 2.

## Acceptance

- In the editor, F5 on Daow 4.4 opens this client on that map, against a
  local AzerothCore, with its units placed and its triggers running.
- Closing the client returns to the editor; the goodbye receipt is read
  and shown.
- A stock unit's model shows for a player with WC3 installed, and a
  wire-frame for one without.

## Decisions (2026-11-04, step 1, answered here; the owner may overturn any)

1. **The contract above is kept as written with world-edit-to-execute's
   side (913)**, with one addition the code needs: the client's own
   `--maps` and `--client`. Nothing in the contract is changed; the
   editor's launcher points at `scripts/run`, which adds them, or adds
   them itself.
2. **Every argument is checked, in every run.** A Test Map launch is a
   program talking to a program, where a misspelt option passed over is
   a silent wrong run; and fallbacks are warnings, and warnings errors
   (docs/001, "Failure model"). So an unknown option or one short of its
   values now stops any run, not only a test map's. The client's own
   options are listed once, beside the loop that reads them, and handed
   to 067 to check.
3. **A test-map run with a receipt always gets one, unless it crashes.**
   067 hands back the handle even when an option is refused, as long as
   `--receipt` was read, so the receipt says what was wrong; every
   failure of 043's start goes through one `start_failed` that writes
   it. A receipt that could never be written (its folder missing) is
   found before the window opens, when the editor can still see the
   message on the terminal.
4. **The resolver opens only in a test-map run, for now.** Opening it in
   every run would make a broken pack named in the settings stop
   ordinary runs, which is 105a's own step to take when the readers ask
   it (204 first). In a test-map run it opens right after the archives,
   so a map folder that isn't there stops the start by name, and its
   counts are printed at the end. `--assets DIR` names where the
   settings' packs and overlays are; without it, settings naming any
   are refused rather than looked for in the wrong place.
5. **`--camera` moves her start.** The camera in WoW mode follows her,
   so placing it is placing her, facing as at any start, the camera
   behind her. When 1101's RTS camera exists, a test map's camera is
   centred on (x, y) instead. A point with no ground under it stops the
   start with a receipt, as any start with no ground does.
6. **The password is required in every test-map run**, as the contract
   says, though nothing logs in yet, so the editor's launch is tested
   whole now. Empty counts as missing. It is copied onto the handle,
   wiped when the handle is freed, never written in a sentence or the
   receipt, and `unsetenv` takes it out of the environment so nothing
   the client starts later (907's model, a browser) inherits it.
   `--account` stays optional, as the contract has it; a login with
   none says so when there is a login.
7. **The receipt's times.** `started` is when the options were read
   (the client's start, before the window), `ended` when the receipt is
   written; both are the machine's clock in UTC to the second, and
   `seconds` comes from the steady clock, so a clock change during the
   run doesn't skew it. Seconds are written to the millisecond.
8. **The receipt's strings** are written in double quotes with a
   backslash, a quote and every control byte escaped (`\n`, `\r`,
   `\t`, else three digits, so a digit after it can't join it); other
   bytes as they are, so a UTF-8 path comes back byte for byte. The
   comment at the top names the file for a person who opens it.
9. **The receipt is written with the rights of an ordinary file**
   (0644), not `mkstemp`'s owner-only ones, and flushed to the disk
   before the rename.
10. **`--realm` defaults to `127.0.0.1:3724`**, AzerothCore's usual
    login port on the editor's own machine (913: the editor starts the
    server there). The host is taken up to the last colon; an IPv6
    address isn't supported.
11. **Exit statuses** as listed above: 2 for the options, as the usage
    message always gave; 1 for other failures.

## Decisions (2026-11-04, step 1b, answered here; the owner may overturn any)

12. **`map.lua` names the map, and only it.** The converter knows which
    map it made; a `--map` the editor passed could disagree with the
    folder. So `--map` with `--test-map` stops the start (a usage error,
    2), and Map.dbc's row must name the folder `map.lua` names (any
    capitals), or the start stops naming both.
13. **A table is every place's copy, read per row**, as 106 asks: the
    first copy holding a row's number answers it. A copy of another
    width is refused, not read, since its columns would be misread
    (106's acceptance, by comparing the copies' widths: there are no
    layout files yet). A row nobody has is still an error naming the
    table and the row, not a placeholder: 106's "unknown spell 133"
    needs its layouts first, and a made-up Map row would open nothing.
14. **Tables and ground files have no placeholder** in the resolver. A
    checker or a box stands in for a picture or a model and is still
    something to see; a made-up table or ground tile would be wrong
    data, so no place having one is the answer (`W_ERR_NOT_FOUND`),
    with no warning or count of a placeholder.
15. **Heights from the ADTs only when the server has none.** The
    server's map files are what the server will check her against; when
    they have the start's tile (W02d has run), they are used, even in a
    test-map run. Otherwise the ADT's heights are turned into the
    server's tile the way the extractor does it, so the heights agree
    with what W02d will write. A server tile that is there but broken
    stops the start rather than falling back.
16. **The slope from a stock map.** The server's walkable-slope rule is
    one number for all maps (its navigation tiles all carry the same),
    so a map with no tiles yet takes the first stock map's that has
    them (1, 0, 530, 571), and says which.
17. **The archives and the resolver open before the ground**, before
    the window: the ground's readers now read through them. Nothing that
    needs a window happens before.
18. **A map's own WC3 model is told by its name and its bytes.** A
    CreatureModelData path ending in `.mdx` is asked for as written,
    folder first; read as MDX only when its bytes start `MDLX` (so a
    pack's `.m2` under that name still works). The game's own models,
    which W02c also names `.mdx`, aren't in the folder and go on to the
    `.m2` in the archives, as before. The model's size: the look's
    scale times `map.lua`'s `wc3_to_wow.scale` (yards per WC3 unit).
    Its pictures come through the resolver (the checker stands in for
    a missing one); its replaceable pictures (team colour, 404) are
    left off; its layers that start hidden (alpha 0 at rest: a death
    or alternate part) aren't drawn. It stands in its rest pose until
    WC3 posing (402 step 4) lands.
19. **PR #2's MDX reader is merged into this branch**, since 041 needs
    `mdx_read`, and this branch is stacked on #4 (and #1). Until #2
    lands, this branch's diff shows #2's files too; once it does, a
    merge of the base makes them drop out.

## Decisions (2026-11-05, step 1c, answered here; the owner may overturn any)

20. **The placements are 1109's step 1c**, not a new issue: they are
    105a's "drawing parts ask the resolver" and 204's pictures, done for
    the one run that opens a resolver. 105a, 204, 302, 401 and 404 say
    what they took and what is left.
21. **Still only in a test-map run** (decision 4 kept). With no
    resolver, every name is read from the archives alone as before: a
    model they lack is left out, quietly, and no box or checker stands
    in, so an ordinary run draws what it drew.
22. **A placed `.mdx` is told the way a creature's is** (decision 18):
    as written, folder first, never a placeholder; WC3 only when its
    bytes start `MDLX`; else the `.m2` beside it. Its size is its points
    times `wc3_to_wow.scale`; the placement's own scale is the
    converter's, used as written (W02b keeps WC3's scale there).
23. **A model is one place's, whole.** When a model's `.m2` is found but
    its view isn't (or the other way round), both are the box's, said
    once as standing in "with the rest of its files", so no model is
    drawn with another's view.
24. **The box is drawn as a magenta wire-frame**, so a missing model is
    seen as missing (105a's wire-frames), and counted at the start. A
    building no place has is the box stretched to the MODF entry's size
    (its widest side across, its height up), so the gap has its shape.
25. **A WC3 doodad stands still and is walked through**: no skeleton,
    particles or solid shape until 402 step 4, 403 and a reading of its
    CLID shapes. Its team-coloured pieces are left off (404) and its
    pieces hidden at rest aren't drawn, as for creatures.
26. **MCRF isn't read.** It says which of a tile's chunks list each
    placement, for a drawer that culls by chunk; 042 places every MDDF
    and MODF entry of a tile once and culls each by its own ball, which
    gives the same picture.
27. **WC3's tilesets wait** (302): they need PR #3's `.w3e` reading and
    a ground shader for WC3's blend; this slice only makes the ground's
    pictures ask the resolver, so a tileset becomes a place there later.
28. **Team colour waits** (404): no player is chosen for a doodad, and a
    unit's colour needs its owner from the server.
29. **034's prefetch still warms the archives alone**, and 040's water
    and 041's game models still read them: none of them stands in a
    placeholder, and a converted map's water and creatures still draw.
    Moving them is the next slice of 105a. (040's water moved in step
    1k, decision 86, never as a placeholder; 041's models in step 1l,
    decision 94; 034's prefetch in step 1m, decision 99.)
30. **A model 061 refuses is left out, said**, as any unreadable file
    is; 061 isn't changed here (open item below).
31. **The tests read the converter's real output when given it**, as
    step 1b's did; with no build folder those checks are skipped, and
    the made folder's checks still run.

## Decisions (2026-11-05, step 1d, answered here; the owner may overturn any)

32. **The WC3 install is the Blizzard install's second half**, asked
    after the game's own archives (a name both hold is the game's: the
    game this client is), under the same place name ("blizzard"; an
    answer's `from` names the archive). The bridge's order is kept:
    packs, the server's files (and overlays, the test map's stand-in for
    them), the player's Blizzard install, then the placeholder. The
    baseline set has no place until the asset forge makes it; it goes
    between the install and the placeholder. (Since step 1k it has
    that place, and its smallest form: decisions 81 to 88.)
33. **Where the install is: `assetWc3`**, a setting row beside the
    resolver's others, holding one folder name under the profile's
    assets (default `wc3`), not a path: the other rows name folders, not
    paths, so a profile's settings never point outside its assets, and
    a value holds 63 characters. The player links `assets/wc3` to their
    install (as `assets/server-data` links to the server's). Missing:
    said once and left out, since most players have none. Reforged's
    CASC storage isn't read (no archive of these names).
34. **PR #3 merged here with a merge commit**, as decision 19 did for
    PR #2: this branch needs `w3e_read` and `w3e_prep`. Until #3 lands,
    this branch's diff shows its files too.
35. **The map's own ground comes from `war3map.w3e` at the build
    folder's root.** The converter doesn't copy it there yet, and
    `map.lua` names no source map (only `conversion-report.lua` does,
    as a path relative to where the converter ran, which the client
    can't rely on). What world-edit-to-execute's W02 should add: the map
    archive's `war3map.w3e`, byte for byte, written to the build
    folder's root (and, if it wants to say so, a `wc3 = { ground =
    "war3map.w3e" }` field in `map.lua`, which this reader would then
    follow). Until then the tests copy it in, and a real run keeps the
    converter's pictures, saying why. (2026-11-05: world-edit-to-
    execute's draft PR #246 makes `convert` copy it there.)
36. **No ground shader.** One WC3 cell is one cell of a chunk and the
    ground's pictures already repeat once a cell, so WC3's laying fits
    038's four layers and blend picture: 033 hands it different picture
    names and strengths, and 038 is unchanged. (Lifted in step 1g: the
    ground's shader lays WC3 cells one by one, decision 57.)
37. **Four tilesets a chunk, the four used most.** 038 draws four layers
    a chunk; where a chunk's cells use more (common in War in the
    Plaguelands: 90.4% of its points are as WC3 lays them), a cell's
    dropped lowest lets its next kept tileset lie whole and a dropped
    higher one is left out. Lifting it is a shader's job (302). (Lifted
    in step 1g: every tileset a chunk's cells lay shows, decision 57.)
38. **One picture a tileset: its whole tile**, with the subtiles' alpha
    for the blends. WC3's 16 variations and its subtiles' own colours
    wait for that shader. Masks are read at the blend picture's 8
    points a cell side. (Lifted in step 1g: a picture's own subtiles,
    alpha and colours, read per point, and each cell's variation:
    decisions 59 and 60.)
39. **Cliffs and blight are laid whole** with the cell's south-west
    point's tileset (063 leaves them bare for the renderer) until
    cliff models are seated (302) and blight is drawn. (Since step 1e,
    a cliff cell with a model takes its cliff's ground: decision 47.)
40. **WC3's tilesets only when the player has them.** With no
    `Terrain.slk` in any place, the converter's own pictures (the game's
    tilesets it picked) stay: a player without WC3 sees what they saw
    before rather than a checkered ground. A tileset the sheet lacks,
    or whose picture no place has, is the checker (said).
41. **077 is a reader library part** (the C twin of `slk.lua`), so
    world-edit-to-execute can call it through the FFI once it is held to
    the Lua's results over the stock sheets (on the owner's machine).

## Decisions (2026-11-05, step 1e, answered here; the owner may overturn any)

42. **Cliffs are doodads.** Each cliff is a placement added to its
    tile's doodad list (078's `wc3_ground_place_cliffs`, called by 033
    beside the painting), so 042 loads, instances and draws it as any
    WC3 model and 076 finds it through the resolver: no cliff drawer of
    its own. Its id is `WC3_CLIFF_UID` (0xC0000000) plus its cell, past
    any id the converter gives, and it is placed by the one tile its
    cell's centre is on. Turned as the converter turns a WC3 doodad
    facing 0 (rot 0, 90, 0), size 1 (076 sizes a WC3 model by the
    map's rule), z by `map.lua`'s `wow_z = scale * wc3_z`.
43. **The model's name as the published map viewers build it**:
    `Doodads\Terrain\<cliffModelDir>\<cliffModelDir><letters><v>.mdx`,
    letters north-west, north-east, south-east, south-west, each A, B
    or C over the lowest corner; a corner more than two levels up cut
    to C (counted as steep; neither real map has one). The variation is
    the south-west point's, or the highest below it that some place has,
    else 0 (a model no place has at any variation is the box, as any
    missing model). Each path is asked of the resolver once, at the
    start. From memory of the viewers' rule, not checked against WC3's
    archives here (open question 9).
44. **CliffTypes.slk's columns by any capitals** (077's new
    `slk_value_any_case`: `cliffModelDir`, `texDir`, `texFile`,
    `groundTile`), since their exact capitals are known here only as
    the published tools spell them; Terrain.slk's `dir` and `file` are
    still read exactly, as before.
45. **A cell's cliff tileset is its south-west point's**, as the
    viewers take it, else the first of its south-east, north-west and
    north-east points' that the map lists (Master Crafters has 123
    cliff cells whose south-west point names none); with none on any
    corner, no model and its ground as before (121 cells there). A
    cliff tileset the sheet has no row for: models from `Cliffs`, its
    picture `TerrainArt\<id>.blp`, which no place has: the checker.
46. **A cliff stands unbent, one cell east of its cell's south-west
    corner, at its lowest level plus its corners' mean height.** WC3
    bends each of a cliff's points to the ground height under it; 042
    draws one mesh for every copy of a model, so a cliff is placed
    whole instead. On the real maps its foot sits at most 2.6 yards
    below its lowest corner (the editor lowers raised corners' own
    heights), less than a level. Bending is a next step: a model per
    cliff, or a shader reading the ground's heights. The cell east is
    the viewers' rule for WC3's cliff models, which lie west of their
    origin: unchecked here (open question 9). (Bent since step 1h:
    decisions 64, 65.)
47. **The ground under a cliff is its cliff tileset's `groundTile`**,
    laid whole (added to the map's tilesets when its list lacks it, as
    the viewers do); a cliff tileset with no ground, or a cliff cell
    with no model, keeps decision 39.
48. **Ramps wait.** A cell on different levels with a ramp corner gets
    no model (980 in Master Crafters, none in War in the Plaguelands);
    the converted ground's slope shows there, laid with 063's pictures.
    WC3's ramp models (`rampModelDir`, their own letters) are the next
    step, with the bending of decision 46. (Done in step 1f: decision
    51.)
49. **A placed name may name its replaceable picture**:
    `<model>|<n>=<picture>` (076's `PLACED_REPLACE_MARK`), so one cliff
    model in two cliff tilesets is two models to 042, each with its own
    picture, and the drawer needs no knowledge of cliffs.
50. **The tests' build folder and war3.mpq are separate arguments**:
    the build folder stays the third ("-" for the default), war3.mpq
    becomes the fourth, as the usage line in `tests/044-test-client.c`
    says (both were the third, so a build folder given broke the
    archives' WC3 checks).

## Decisions (2026-11-05, step 1f, answered here; the owner may overturn any)

51. **A ramp's model on each ramp cell that changes level.** A cell on
    different levels with a ramp corner takes
    `Doodads\Terrain\<rampModelDir>\<rampModelDir><letters><v>.mdx`
    (`CliffTrans` when the sheet has no such column or cell); its
    letters as a cliff's (north-west, north-east, south-east,
    south-west) but a ramp corner's `'L' - 4` a level (`L` lowest, `H`
    one up, `D` two), as the published map viewers name WC3's ramp
    pieces; its tileset, variation, picture and placing are the cliffs'
    (decisions 43, 45, 46). Master Crafters' ramps are three points
    wide (a level, then two ramp points on the lower one): the lower,
    flat cell gets no model, its neighbour's reaching over it, the way
    the viewers draw a two-cell ramp piece. From memory of the
    viewers, unchecked here (open question 10).
52. **A ramp's cell keeps its own ground**, laid as 063 lays a textured
    cell, not its cliff tileset's ground: a ramp is walked ground, and
    the model gives its sides.
53. **The blight's picture from the editor's data**:
    `UI\WorldEditData.txt`'s `[TileSets]` row for the map's main
    tileset (its letter, `A=` or `A = `), its second comma-separated
    field, `.blp` added when it has no ending (as the viewers read it).
    No such file or row: blighted cells laid as before (decision 39).
    The few lines that read it are 078's own; a general reader for
    WC3's INI-like text files waits for a second user.
54. **Blight is a tileset of its own, the last**, laid on each blighted
    corner by the tilesets' own corner rule (a corner's tileset is the
    blight when it is blighted), so it blends into the ground round it
    as WC3's does; a cliff cell's cliff ground (decision 47) wins over
    it. Its picture is laid out as a tileset's (4 x 4 subtiles; 8 x 4
    when twice as wide).
55. **The counts keep cliffs and ramps apart** (`ramps`,
    `ramps_placed`, `ramps_no_tileset`); `steep` and the models' counts
    take both.
56. **The tests' colours repeat past 16 tilesets**: War in the
    Plaguelands' blight is its 17th tileset and shares the first's
    colour in the made pictures, so its blight's laying is checked
    less sharply than Master Crafters' (its 11th). (Gone in step 1g:
    decision 62.)

## Decisions (2026-11-05, step 1g, answered here; the owner may overturn any)

57. **A ground shader that lays WC3 cells one by one** (lifts
    decisions 36 to 38). Chosen over splitting a chunk into draws of
    four tilesets each (more draw orders, and a cell's blend split
    across them) and over more layers in 033 (more blend pictures a
    tile and still a cap, and each variation would be a layer of its
    own). Each cell's pieces ride in one small picture a tile (256 x
    128, 128 KB), read texel by texel; the tileset pictures sit in the
    ground's picture array as any ground picture does. WC3 chunks use
    the blend picture's shadow only; the four-layer blend stays for
    every other chunk.
58. **The shader in its own window-free file (080), as C text**, not a
    `.glsl` file read at run time: the client is one executable with no
    files beside it but its data, and the tests and a checking tool
    read the same text the card gets. One shader for both kinds of
    tile: a tile with cells (a uniform) branches per point.
59. **A tileset picture as one layer, or two when extended** (its two
    halves, each 4 x 4 subtiles), named by its path and the mark, with
    `|right` for the right half; the array's 256 x 256 (a WC3 tileset
    picture is 256 high). A piece is read a half texel in from its
    subtile's edges, with the slopes of the cell's own place, so the
    card picks the same smaller copy on both sides of a cell's edge.
    The smaller copies still mix a subtile's edge with its neighbours'
    from far off.
60. **Each cell's variation from its south-west point**, through 063's
    `w3e_variation`, for every kind of cell (textured, a cliff's
    ground, blighted, bare). 063's rule for a plain picture (variation
    0: subtile 15; otherwise subtile 0) is its Lua twin's; the published
    viewers may read it the other way round. Both are whole tiles (open
    question 11).
61. **A cell off the WC3 grid in a laid chunk** takes the tileset the
    chunk's cells use most, whole (subtile 15), and the chunk's own four
    pictures name it too, for a drawer without the cells. (Before, it
    showed the chunk's layer 0.)
62. **The tests' made pictures colour each subtile its own** (up to 32
    tilesets of 32 subtiles each), so a wrong variation or piece shows,
    and the blight's colours are its own (decision 56 gone). The tests
    check each cell's pieces against WC3's rule worked out from the
    points, and every blend point's colour as the shader works it out
    (its sums done in C, on the pictures as 033 unpacks them).
63. **The GLSL checked outside the tests**, on Mesa's software
    renderer under a virtual X display (both already on this machine;
    nothing installed): the test program has no window and links no
    OpenGL, so the drawing check stays a tool run by hand, its result
    recorded here.

## Decisions (2026-11-05, step 1h, answered here; the owner may overturn any)

64. **A cliff bent on the CPU when its model is read, the bend carried
    in its name** (lifts decision 46's "unbent"). Chosen over a shader
    reading the ground's heights: 042 draws one mesh for every copy of
    a model, so a shader would need each copy's ground in a picture or
    uniforms per draw, and could be checked only with a window. A bent
    cliff is a model of its own (206 on War in the Plaguelands, 1011
    on Master Crafters, beside the flat ones' shared models), read once
    like any other; the name keeps 076's one way in (decision 49's
    marks), and any place's model is bent the same. Its normals are
    left as the model has them.
65. **The bend is WC3's rule as the published viewers read it**: each
    of a model's points raised by the ground's height under it (the
    tile points' heights alone, levels left out, blended between the
    four round it), less the corners' mean the cliff already stands
    at; read from a 4 x 4 grid round the cell, since a cliff model
    reaches past its cell (one west: decision 46), clamped at the
    grid's edge. Kept in sixteenths of a unit, whole numbers, so the
    name is short and exact. Any other model in a bent name (the box
    standing in) is left unbent.
66. **The water's surface stays 063's** (the stored level less 89.6
    units, as the converter writes it), and a depth is that less the
    ground (levels counted), in cells. The sheet's `height` (the
    surface's offset WC3 reads from it) is read and kept, not used: if
    a real `Water.slk` gives -0.7 (89.6 / 128) the two agree; if not,
    the plan's step 4 moves the surface.
67. **WC3's water is a plan of four steps, not one slice**: today's
    water comes from the converter's ground files and the game's
    tables through the archives alone, and the map's own tint lives in
    a file the build lacks.
    1. Read WC3's water without a window (081). Done in this step.
    2. Draw it (040, 043): a converted map's water (its ground files'
       kind 1) with 081's frames as its ripple pictures, `texRate` a
       second, and each point coloured by 081 at its WC3 point (its
       colour and alpha in place of the zone's tint and opacity); when
       081 finds no water, as now. A new shader text, checked on the
       software renderer as decision 63's was.
    3. The map's own tint: world-edit-to-execute copies
       `war3map.w3i` to the build as it copies `war3map.w3e`, a reader
       for its water tint (its flag and colour), and 081 tints by it.
    4. The converter's wet cells and WC3's: War in the Plaguelands'
       ground files mark 8087 wet cells against 4627 wet tile points
       (the converter's rule for a wet cell unread here; WC3 draws
       every cell with a water-marked corner, hidden where the ground
       is higher). Check
       040's mesh against the points, and draw from 081's points where
       they differ; the surface by the sheet's `height` if decision 66
       needs it.
68. **All of the row's frames or no WC3 water.** A frame no place has
    (the install's, a pack's or the map's own) means the converter's
    water stays, never the checker and never a mix with the game's
    pictures. A row's colours must be whole numbers from 0 to 255, its
    `numTex` from 1 to 100 (frame numbers are two digits); `texRate` 0
    or missing reads as one a second.
69. **The water section of the tests makes its own sheet and frames**
    (no Warcraft III install here, as for the tilesets: decision 62),
    each frame its own colour so a wrong frame shows, and checks the
    real conversions' points against the rule worked out in the test
    from each point's stored numbers.

## Decisions (2026-11-05, step 1i, answered here; the owner may overturn any)

70. **Which water is WC3's.** In a test-map run where 081 opens, every
    water or sea layer (LiquidType type 0 or 1) of the ground is drawn
    as WC3's: a converted map's ground holds only its own water (kind
    1). Lava, slime and buildings' pools keep the game's look. With no
    WC3 water (no install, sheet, row or frame), everything as before.
71. **The water shader in its own window-free file (082), with the
    layer's points**, as decision 58 did for the ground: the test
    program checks the very points and text the card gets. One shader,
    a uniform (`wc3`) choosing WC3's branch: the picture times the
    point's colour, alpha too, unlit (as the viewers draw WC3's water),
    fogged by the zone's fog as everything else is. The frames are
    shown at the sheet's `texRate`, not the game's 15 a second.
72. **The map's tint multiplies each colour**, red, green, blue and
    alpha each times the tint's over 255, and only when the map's flag
    0x10000 ("use water tinting colour") is set; the bytes read red,
    green, blue, alpha, as the published format notes give them
    (world-edit-to-execute's Lua reader names them blue, green, red:
    open question 13). A `war3map.w3i` 083 can't read stops WC3's water
    with its reason, as a broken `war3map.w3e` stops WC3's ground,
    rather than drawing it untinted unsaid.
73. **Each drawn point at its own WC3 surface** (the plan's step 4).
    On both real maps the converter's wet cells on the grid are exactly
    WC3's cells with a wet corner (WC3's other water-marked cells lie
    wholly under the ground), and its height at every wet corner is
    WC3's; only a wet cell's dry corners differ, where the converter
    keeps the wet level and WC3's surface dips under the bank (31
    points on Master Crafters, none on War in the Plaguelands). So the
    drawing takes WC3's surface at every tile point; water past the
    map's edge (War in the Plaguelands' 2624 cells south of its grid,
    at its sea level) keeps the file's height and takes the colour of
    the grid's nearest point. The mover's water (027) stays the
    converter's: where one swims is the server's ground files'.
74. **083 reads only what the water needs, and every version WC3's
    maps use**: 18 (Reign of Chaos: flags and tileset; no fog, no
    tint), 25, 28 and later (the game's version, four numbers, after
    the editor's from 28: the published notes; HiveWE may read it from
    27, which no map uses). It stops at the water tint; a file that
    ends before it is refused, naming the field; 19 to 24 (betas) and
    below 18 refused as not supported. No handle: a plain record, in
    READERS (libwreaders.so too).
75. **The map's fog is read and kept (083, `wc3_water_info`), not
    drawn**: the world's fog is the zone's (026, 037) for the sky,
    ground and water alike, and a converted map's own fog belongs with
    its lighting, a step of its own. Both real maps set fog style 0
    (none). (Since step 1j drawn, and style 0 read as linear, turned on
    by the flag 0x2000: decisions 76 to 80.)

## Decisions (2026-11-05, step 1j, answered here; the owner may overturn any)

76. **The map's fog is on when its flag 0x2000 is set, and its style
    counts from 0** (linear, exponential, exponential squared: the
    editor's list). The published notes as recalled and
    world-edit-to-execute's reader and format notes take style 0 for
    no fog and name no flag for it (0x2000 "purpose unknown"). Of the
    18 maps at hand whose file was read (world-edit-to-execute's
    assets: 19 maps, DAoW 6.2's not taken out by the scratch tool), all
    version 25 and all style 0, War in the Plaguelands alone sets
    0x2000, and it alone has fog settings other than the editor's
    defaults (start 3500, density 0.4, colour 30, 120, 255, where 15
    others keep 3000 to 5000, 0.5 and black, and two keep 3000 to 5000
    with a magenta colour and density 0.25 but no flag). Read the notes' way, no map
    here has fog and War in the Plaguelands' settings go unused; read
    this way, the one map whose author set a fog shows it. The flag's
    name, "use terrain fog", is recalled rather than read here (open
    question 13).
77. **Distances from the eye, in yards by the map's rule; the colour
    red first.** The file's start and end are WC3 units from the
    camera, times map.lua's `wc3_scale` (1/30.72 on both maps), as
    every other WC3 length in a converted map; a start before the eye
    is at the eye, an end before its start at its start. The colour's
    bytes read red, green, blue as the tint's do (decision 72); its
    alpha and the density aren't used.
78. **Every style drawn linear**, from the start to the end, as every
    shader here fogs: the exponential styles' density (and how WC3
    uses it against the distances) isn't matched; one fog formula
    stays in every shader's text. No map at hand uses them.
79. **The map's fog replaces the zone's colour and distances in the
    light each frame, and nothing else.** Every drawer reads that one
    light, so the ground, water, doodads, their particles, figures,
    the horizon and the clear colour all take it; the sky dome, the
    skybox, the sun and the shade stay the zone's. Under the water the
    zone's underwater light stays (WC3 has no view under its water).
    The view distance still ends the fog at the latest, after the
    map's is put in (173b6). It needs no Warcraft III install: it is
    the map's own data, read in any test-map run.
80. **A `war3map.w3i` that can't be read stops the test-map run**,
    naming it, as a broken `war3map.w3e` stops WC3's ground and as 081
    stops for the same file; with none in the build the zone's fog
    stays, said at the start. 081 and the fog read the file through
    one function (084's `map_w3i_read`).

## Decisions (2026-11-05, step 1k, answered here; the owner may overturn any)

81. **The baseline set is the fifth place, laid out as a pack.** After
    the Blizzard installs (the game's archives, then Warcraft III's) and
    before the placeholder, the owner's order. Its folder holds the
    files at their names and a `manifest.lua` of format 1 (docs/012),
    read by the reader packs use: the asset forge's set can drop in
    with its scores and seals, and the smallest set records its own
    provenance the same way. A name the set lists is answered from it;
    a name it doesn't list goes on to the placeholder (a file there
    that no entry names is said at the start and never served, as in a
    pack).
82. **Where: `assetBaseline`**, a setting row beside `assetWc3`, one
    folder name under the profile's assets (default `baseline`, so
    `assets/baseline`); empty for none. A folder that isn't there is
    said in the log and left out (a player who hasn't made it gets the
    placeholders, as before); one there with no manifest, or a broken
    one, stops the start, naming the setting, as a broken pack does.
83. **Its answers are their own place, not placeholders.** Counted
    under "baseline" (a column of the counts table between the
    installs and the placeholders), no warning, taken by strict mode
    (`assetsStrict` refuses placeholders, and the set is a real answer
    the player can see), and never counted as replaced past the
    distinctness threshold (it has no scores yet; the forge's will).
    The placeholder's warning names the set among the places looked
    in when it is open, and is as before when it isn't.
84. **The smallest set's contents, where none were defined: made by
    code from the converter's tables.** The repositories track no art,
    and the converter's two tables are the list of names a converted
    map can carry (`models.lua`: "the names the baseline set has to
    cover"). The ground: each row of `ground_textures.lua` with a `wow`
    (every tileset's grounds and blight, and the fallback) as a 64 x 64
    plain BLP2 of the row's own `rgb`, the colour the converter already
    uses for it, mottled by a smooth wrapping noise (12% deep, a little
    grain) seeded by its folded path, so it reads as ground, tiles with
    no seam and is the same on every make. A path named twice keeps the
    first row's colour.
85. **One stand-in model for every doodad; buildings and creatures
    left out.** Each `.m2` path in `models.lua`'s `doodads` (types, by
    model, classes) and its `00.skin` is answered by one shared file
    pair: a grey eight-sided post, a yard across at its foot, 0.8 at
    its top, two yards high, an M2 of version 264 with one opaque unit
    drawing its own picture (`Baseline\StandIn.blp`, grey stone with
    a blue band: the set's own name, so it never stands for a game
    picture). It is visibly not the original and visibly not the
    magenta box. A `.wmo` row (a building is a root and its groups) and
    the `units` rows (a creature needs a skeleton and animations) are
    left out and counted; creatures stay the box.
86. **Water: LiquidType 1's run, as recalled.** The converter writes
    one liquid, LiquidType 1 (water); its 30 frames are named
    `XTextures\river\lake_a.1.blp` to `lake_a.30.blp`, recalled from
    3.3.5a rather than read here (no client); a wrong recollection
    leaves the water with no frames, as before. Each frame is a 64 x 64
    blue ripple that moves a step a frame and repeats over the run. 040
    now reads its frames through the resolver (`asset_read`), so the
    set's answer.
87. **Made by a program, not committed.** `make-baseline FOLDER
    GROUND_TABLE MODEL_TABLE` (086, built by `scripts/build`) writes the
    set; `assets/` is git-ignored, as every other asset folder is, and
    the 4.4 MB of made files would only repeat what the code makes.
    `scripts/run` makes it once when `assets/baseline` isn't there and
    the tables are (`WETE`, default world-edit-to-execute's folder). A
    make refuses a folder holding files but no manifest (someone
    else's), a path that climbs out of the folder or names the
    manifest, and a ground row without three whole numbers from 0 to
    255; it writes the manifest last, through a new name, so a make
    that stops part way leaves the last whole one.
88. **What the smallest set doesn't cover.** The client's tables
    (`LiquidType`, `Light`, `Map`, `AreaTable` ...), her model and the
    creatures still come from the game's archives, so a run with no WoW
    install still stops at the start; buildings, WC3's stock pictures
    and models (named by a map's own WC3 models), sounds and the
    interface stay placeholders without an install. Each is the asset
    forge's, or a later step's. (Since step 1l the tables a start needs,
    her and the creatures come from the map's folder and the set, and a
    test map starts with no install: decisions 89 to 97.)

## Decisions (2026-11-05, step 1l, answered here; the owner may overturn any)

89. **A test map may start with no WoW install; an ordinary run may
    not.** A test map's world is its own folder and the baseline set,
    so an empty Data folder is said ("no WoW install: ...") and the
    chain is left empty: every read from it is "not there", and each
    part already goes on to the next place. A run on the game's own
    maps has nothing else to draw, so it still stops, naming the
    folder. With neither an install nor a set, the start is said
    ("test map: no install and no baseline set") and stops at the
    first table it needs (`Light.dbc`), named.
90. **The set holds the tables a start needs, made by code.**
    `LiquidType` with one row (1, "Water", type 0, its texture pattern
    the set's own water run, so the converter's one liquid has its
    frames), and the five light tables empty, as wide as 3.3.5a's
    (recalled: 45, 15, 34, 34, 9 and 3 fields). Empty light tables mean
    no zone light, so 043's plain grey day, which reads as "not the
    game's" the way the post does; a made-up sky would claim a look
    nobody chose. Written at `DBFilesClient\NAME.dbc` with entries of
    kind `table`.
91. **Tables per row, the set's last; a set's copy of another width
    left out.** 040 reads `LiquidType` through 074, so a row the map
    or a pack has wins and the set's row answers only ids no one else
    has. The light tables are read whole as the first copy (026 scans
    them, it doesn't ask by id). Since the widths are recalled, a set's
    copy whose width differs from the first copy's is left out and said
    on the terminal, rather than refusing the table as two other
    copies of different widths still are: a wrong recollection costs
    the set's rows, never the start.
92. **No navigation tiles in a test map: 60 degrees.** The steepest
    walkable slope comes from the map's own navigation tiles, then the
    server's for the stock maps; a test map with neither takes the
    server's rule, 60 degrees, as its tiles for Kalimdor hold it
    (044 checks that number against them when they are there). The
    server builds every map by one rule, so this is its number, not a
    guess.
93. **One stand-in figure for her and for a creature with no model.**
    An eight-sided sand column, 0.3 yards in radius and 1.8 high, with
    a blue band down the face that looks along +x (the way a model
    faces, so which way it turns can be seen), one bone that never
    moves and one animation, Stand, its moments in the `.m2` (no
    `.anim` file). The box would stand for a model that failed; this
    stands for one nobody has, at a person's size, so a creature still
    stands where it should and can be clicked. Stand only: she slides
    standing when she moves, said at the start as any missing
    animation is.
94. **041 reads every file through the resolver.** Its models, views,
    `.anim` files, pictures and items, with `asset_read` (the first
    place that has it, never a placeholder: a figure decides itself
    what stands in). Which files a display row means is 088's, out of
    the window and checked. A model only the set answers is the set's
    skeleton-less post (decision 85), which a creature can't stand or
    walk with, so the figure answers instead. A humanoid display (a
    character look) that can't be built in a test map takes the figure
    too.
95. **The start factored out of the window (087).** So 044 can run
    043's start path, the archives to the slope, against a real build
    with no install, as the brief asked; 043 calls it and holds its
    result. The window, her figure and everything drawn come after it
    and stay unchecked here.
96. **`scripts/run` in a test map: no server folder, no `common.MPQ`,
    a note.** Both were errors before any run; a test map needs
    neither, so each is said as a note and the client decides.
97. **What still can't work without an install.** Her real character
    (`CharStartOutfit`, `CharSections`, the race models and skins: the
    figure stands in) and the bystanders (left out, each said); a
    creature's own look when the map didn't bring it (the figure);
    buildings (WMO), WC3's stock models, ground tilesets and cliffs
    (the converter's pictures stay); sounds, the interface and the
    skybox; zone light (the grey day). Each is the asset forge's set or
    a player's install. The window itself, and anything drawn, isn't
    checked here.

## Decisions (2026-11-05, step 1m, answered here; the owner may overturn any)

98. **025 reads through the resolver; a table whole, the first place's
    copy.** Her tables are scanned (by race, sex and choice, or the
    whole of `ItemDisplayInfo` for a look), not asked by number, and
    074 merges by number, so each is read whole from the first place
    that has it, as 026 reads the light tables. A converted map that
    brought part of one of them would hide the archives' rows; none
    does today, and a merged scan is 074's to add when one does
    (added in step 1q: decision 128, which lifts this limit). Never
    a placeholder: a missing table or model is said by name, a missing
    layer picture left unpainted, as before. Errors now say "no place
    has" with a resolver and "the client's archives hold no" without.
99. **034 reads ahead through the resolver, as the load reads.** The
    same names and kinds the tile's load asks (pictures as pictures,
    models and buildings as models), so a file read ahead is the file
    the load gets; never a placeholder, so no warning and no box for
    a file read ahead. A name ending `.mdx` is read as named first (a
    converted map's own WC3 model, with its pictures); otherwise the
    `.m2` beside it, as 076 does. Files read ahead count in the
    resolver's counts as the load's would (once per asset). In an
    ordinary run (no resolver) it reads the archives as before; a game
    `.mdx` name costs one more lookup that finds nothing.
100. **A character that fails part way lets go of what it made.**
    `character_build` frees its model, pieces and pictures on any
    failure and leaves `out` empty, so a caller has nothing to free
    (before, a failure after the model was read kept it).
101. **What still reads outside the resolver stays, each for a
    reason.** The archive reader (015), the resolver's Blizzard place
    (060), the start opening the chain (087), the dressing room (058:
    its own program, no resolver, an install required), and the
    server's own files (018 heights, 019 navigation tiles), the
    creature list (035) and the settings (014), which are not the
    game's archives.

## Decisions (2026-11-05, step 1n, answered here; the owner may overturn any)

102. **`--headless MOVES` is a test-map option.** The headless run is
    for checking a test map, so it is read with the test map's options
    (067) and refused outside a test-map run, as `--camera` is. The
    password is still required: a test-map run's contract is the
    editor's, which always gives it, though nothing headless reads it.
    `--shot` with it is refused (one draws, the other opens no window).
103. **The moves are a Lua data file, format 1, strict.** The same
    data-only Lua the client already reads (`map.lua`, manifests, the
    converter's tables), so the editor can write one as it writes the
    others. A field or key it doesn't have is refused by name and line
    rather than passed over, so a misspelt move can't make two runs
    look alike by doing nothing. The project's list lives in `tests/`,
    numbered (090), beside the test program that uses it, as the other
    tests' fixtures do.
104. **A fixed step, 0.05 seconds by default; seconds rounded to whole
    steps, at least one.** The `--shot` pictures' 20 steps a second.
    A step is the same length every time, so the same moves give the
    same path; at most 1000 moves and 72000 steps (an hour), so a
    faulty file can't run for ever.
105. **The headless world is the ground and the water.** Her ground's
    heights (the server's map files, or the map's own ground files) and
    the water of the tiles within one tile of her (each tile's ground
    file read through the resolver for its liquids, as 043's tile load
    files them). The floors and walls of doodads and buildings are left
    out: they are built from the models by the drawing parts (042), so
    a headless path can differ from a windowed one where she meets a
    model. It needs no window and keeps every count to the map's own
    ground. (The gap is closed by decision 111, step 1o.)
106. **Her figure and the creatures' looks as the window would find
    them.** Her character from 025 (window-free), else the baseline
    set's stand-in, its skeleton read for her animations, chosen each
    step from the same keys as 043 (030, its own fixed seed); each
    distinct creature look through 088 and read as 041 reads it (a
    character look counted, not built: 041 builds those with the
    window).
107. **The receipt carries the counts as named numbers after its
    fields; format stays 1.** The receipt is one flat table (the
    editor reads it with LuaJIT), so each count is a field of its own,
    written exactly (`%.17g`: read back as the same double); a reader
    that doesn't know one passes it over, so no format change.
    `frames` stays 0, since nothing is drawn; `reason` is `closed`
    when the moves end (the run ended as asked) and `error` with `why`
    when it can't start. Exit status 0 for a run whose moves ended, 1
    for an error, 2 for a bad option, as the windowed client's start
    already uses 1 and 2.
108. **Step 5's check, client side: twice in the test program.** For
    each build the tests are given, the project's moves run twice and
    the two receipts must hold the same fields and values but for
    `started`, `ended` and `seconds`; a third run with other moves must
    differ, so the comparison can't pass by seeing nothing. Both runs
    are in one process (the client binary needs raylib, not here);
    each opens its own world, so nothing is shared but the code. A
    windowed run is not compared: its steps follow the frame time.
109. **The editor's side is not here.** F5 running the client headless,
    or comparing its receipts, is world-edit-to-execute's (step 2).
110. **Her start is shared.** Her facing (3.6 radians), speed (120%)
    and look (a night-elf woman, a hunter's outfit) move from 043 into
    087 (`WORLD_START_FACING`, `WORLD_START_SPEED`, `WORLD_HER_*`), so
    the headless run starts her as the window does.

## Decisions (2026-11-05, step 1o, answered here; the owner may overturn any)

111. **The headless world has the placed models' floors and walls
    (closing decision 105's gap).** Each tile within one of her has its
    doodads and buildings placed as the window's 042 places them, into
    the mover's solids and water, from an origin at her start as 043's;
    the mover asks the map's ground and the models' floors, their
    walls, and the water, buildings' pools included. A headless path
    now differs from a windowed one only where the window loads tiles
    farther out (its view reach) or drops them, which the mover never
    meets: it stands within one tile of her.
112. **One part builds the shapes; the drawer keeps its own
    bookkeeping.** The shape of a model and of a building part, the
    box's size for a building, and the floor and wall rule live in 091
    (window-free), and 042 and 043 call them, so the two runs can't
    drift. The placing (once a name, once a placement, furnishings,
    dropped tiles) is 091's own copy of 042's rather than 042 handing
    its solids to 091: 042 reads each model once for its pieces too,
    and handing 091 the same files would read every model twice in a
    windowed run. The two copies are short and each is tested.
113. **The receipt counts the models.** models, models_solid,
    models_wc3, buildings, model_stand_ins, models_left_out,
    model_copies, solid_copies, solid_triangles, building_pools,
    steps_on_models and steps_walled, as named numbers (decision 107),
    so a change in what a map places, or in how she meets it, shows in
    step 5's comparison. A stand-in is the box or the baseline set's
    stand-in for a game model (085).
114. **Moves may name their map.** An optional `map`, the map's folder
    name (no slashes); a run on another map refuses them with
    W_ERR_REFUSED, the receipt saying `error`, since a route found on
    one map is only a route there. Moves with no `map` (090) run on any
    map. Format stays 1: the field is new and optional, and no other
    reader of moves files exists yet.
115. **Each test build has its own route, found in the mover's own
    steps.** The routes (092 for War in the Plaguelands, 093 for Master
    Crafters) were found by a search over turns of a twentieth of a
    second and runs of two or three seconds, stepping the same mover
    over the same ground, models and water as the headless run, from
    each map's start to its water, then (after two seconds' swim and
    two of diving) off its ground's edge, where she falls with nothing
    under her for three seconds. They hold while the map's ground, its
    water and the mover stay as they are; when one changes, the test
    says so (no swimming, or no edge) and the route is found again.
    The search is not in the project: a route is data, written once.
    The test program runs a build's own route when its folder name is
    one of the two, and says it skipped them otherwise.
116. **WC3 models and stand-ins are walked through, as in the window.**
    A WC3 model's own collision shapes (CLID: boxes, spheres) are its
    selection shapes in WC3, not walls (WC3 paths units by its pathing
    map), so they are not seated; the box and the baseline set's
    stand-ins are signs of what is missing, not walls. With no WoW
    install, both test builds therefore have no solid model, and their
    routes' counts are the ground's and the water's; the models' floors
    and walls are checked with a made tile (044).
117. **The edge is where the map's ground ends.** `steps_no_ground`
    counts steps with no ground under her (her ground below -10000);
    off the edge she falls at the mover's own speed, with nothing to
    stop her, until the moves end. The client doesn't hold her at a
    map's edge: the server does (a player can't walk off a real map),
    and a test-map run with no server is where a converter's gap would
    show. (Step 1p: with WC3's pathing as walls, Master Crafters' closed
    border holds her before its edge; decision 121.)

## Decisions (2026-11-05, step 1p, answered here; the owner may overturn any)

118. **Converted maps get WC3's pathing as walls** (the owner's
    decision, through the coordinator): WC3 moves its units by its
    pathing map, not by its models' shapes, so the pathing is WC3's own
    equivalent of collision, and a converted map keeps it.
119. **A wall is land closed to walkers.** A cell with both 0x02 (no
    walking) and 0x40 (land) blocks. Deep water (0x02 without 0x40) is
    closed to WC3's walkers too, but the converted world fills it with
    water deep enough to swim in, and the mover's water decides there;
    flying, building and blight bits are not read for walking.
120. **Past the grid there is no pathing.** world-edit-to-execute's
    parser answers "closed to all" past the grid; here, past it the
    ground's own extent decides (decision 117), so a converter's gap
    still shows as ground missing, not as a wall.
121. **Master Crafters' route ends at its border, not off its edge
    (amends 117 for it).** Its pathing is closed all round (walkable
    cells 24 to 999 across and 16 to 991 up), and a search over every
    reachable place found no way off its ground, so 093 now runs to its
    water and then into the south border, where the pathing holds her;
    the test asks swimming, no step off the ground, and steps held by
    the pathing. War in the Plaguelands' north rows are open, so 092
    still leaves its edge. The routes were found again as before
    (decision 115), with turns to a facing over 72 facings.
122. **No `war3map.wpm`, no pathing walls.** A build from before the
    converter copied it, or a map with none, runs as before: the log
    says so, the receipt's pathing is 0, and the test program skips the
    routes (found with the pathing). Anything wrong with a
    `war3map.wpm` that is there (its tag, version, size against
    `war3map.w3e`'s grid) stops the start, naming the file.
123. **The pathing's walls are counted apart.** steps_walled_pathing
    beside the models' steps_walled, so step 5's comparison tells which
    held her; steps_on_closed counts steps ending in a closed cell,
    which the rule should never allow (0 in every run here), so a leak
    in it shows in the receipt.
124. **`turn_to` is a facing, not a turn.** Absolute radians in the
    game's sense (0 north, rising west), from -2 pi to 2 pi; she faces
    it at once, before the move's steps, as a mouse turn does. A route
    written as facings and runs doesn't drift when the turn rate
    changes, and is shorter to read. Format stays 1 (decision 114's
    reason).
125. **The receipt holds 96 counts.** 1p's five counts passed the 48
    068 allowed.
126. **Both the place she steps to and her front there are asked.**
    Asking only her front let a step slip past a closed cell's corner
    diagonally (seen on Master Crafters' walls route); asking both
    closes it. The wall's facing follows the side crossed, which the
    mover slides along.
127. **0x80 is not read.** It is set on most cells walkers can't enter
    (cliffs, the closed border: new_map.lua writes 0xCE there), but
    neither world-edit-to-execute's parser nor its writers name it, so
    it stays unchecked; 0x01 and 0x10 appear in neither test map.

## Decisions (2026-11-05, step 1q, answered here; the owner may overturn any)

128. **A search reads every place's rows, the first place's first.** A
    table searched by something other than its number is one list: the
    first copy's rows in their own order, then each later copy's rows
    whose numbers no earlier copy has, in theirs. With one copy it is
    that copy, as before; a converted map's row wins a search (it is
    met first) as it wins a lookup, and a row it doesn't replace is
    still found. This lifts decision 98's first-copy-whole limit. The
    list is made the first time it is asked, and kept with the table.
129. **025 searches through 074; 026 doesn't yet.** Her tables are
    read through a Tables handle 025 makes for each call and lets go
    after it, as it read the first copy for each call before, so its
    functions keep their arguments. Where a search kept its last match
    (CharHairGeosets, CharSections) it keeps it within the first place
    that has one, so a later place never overrides an earlier one.
    Their copies must now agree on their width (074's refusal), as for
    a row asked by number. The light tables (026) stay the first
    place's copy whole: their reader hands out the tables themselves,
    and no converted map brings light rows yet.
130. **A placeholder row is the row's number and zeros, where a reader
    asks for one.** Without layouts (106) nothing says what a field
    should hold, so the row is as wide as the table's first copy, its
    first field the number asked, every other field 0 and every text
    "". Only a reader that knows what to make of that asks
    (`tables_row_or_placeholder`): 088, for a creature's display and
    model rows. A lookup elsewhere (her map's Map row, LiquidType) is
    still an error naming the table and the row, since a zero row
    there would mislead rather than stand in. A table no place has
    gets none: there is no width to make one by. (Changed in step 1r:
    a layout fills the row and gives a table no place has its width;
    decisions 136, 137.)
131. **Short-lived: made once, kept in memory, never written.** A
    placeholder row lives with the tables until they are let go, made
    the first time its number is asked and given again after; the
    resolver says it once (a warning naming the table and row) and
    counts it with its placeholders; strict mode refuses it each time,
    an error, and the exit status turns to 1 (106). No resolver (every
    run but a test map's) means no placeholder rows, as before. It is
    an interim measure until layouts can describe a real placeholder.
132. **A creature whose display row no place has stands as the
    stand-in.** 088 gives the baseline set's stand-in figure for a
    placeholder display or model row (its size 1, its skins none),
    said, so a converted map whose creatures name rows it didn't bring
    still shows them standing where they should, in the window and
    with no window. The headless receipt counts looks_placeholder
    (those looks; they count in looks_stand_in too) and
    table_placeholders (the rows made); the windowed receipt carries
    table_placeholders. Without the stand-in (no baseline set) the look
    is missing, as before.
133. **A WC3 creature look may have nothing 061 draws.** The newer
    converter's Master Crafters brings `WaterMagic.mdx` as a creature
    look: particle emitters alone (403's), no geometry. The check of a
    map's WC3 creature models now asks that each is read and that some
    have something to draw, not all.

## Decisions (2026-11-05, step 1r, answered here; the owner may overturn any)

134. **Layouts are C data in the client, for the tables it reads and
    the converter writes.** 097 holds twenty: the converter's seven,
    copied from world-edit-to-execute's LAYOUTS (names and kinds checked
    equal, column by column), and the thirteen this client reads
    (her character's six, CreatureDisplayInfoExtra, LiquidType and the
    five light tables). Columns a reader reads are marked with it;
    the rest are unchecked, as in the converter. One file for both
    projects (the converter reading 097, or 097 written from its Lua)
    waits: ai-stuff isn't changed here.
135. **Another width is said; too narrow is refused.** Every width is
    unchecked against a 3.3.5a file, so a copy of another width than
    its layout's is said on the standard error and read as it is (the
    layout may be the wrong one). A copy narrower than the last column
    this client reads is refused, an error naming its place, its width
    and that column, since a reader would read past its row. Copies of
    a table must still agree with each other (decision 129). This is
    softer than 106's "a mismatch is a hard error", which assumes a
    layout checked against a real file; once one is, the said case can
    become the refusal.
136. **A placeholder's values are the layout's.** Sizes and scales 1,
    CreatureModelAlpha 255, LightParams' water alphas 0.75 (026's own
    for a missing set), a name text "unknown <table> <row>" in its
    first-language text (106's "a visible 'unknown spell 133'") with
    the mask 0xFF01FE (the converter's, unchecked), every other column
    0 or "". Its width is its copies' (they agree) or, with none, its
    layout's; columns past a layout are 0; a table with no layout gets
    the step 1q row (its number, zeros).
137. **Readers that ask for a placeholder: 088 and 041.** 041 now asks
    for its CreatureDisplayInfo and CreatureDisplayInfoExtra rows as
    088 does, so the window's creature from a missing display row is
    088's stand-in, and one from a missing look row the stand-in figure,
    said. Her map's Map row, LiquidType, the light tables and her
    character's tables still error: a made-up row there would mislead.
    A table no place has can take one when a layout gives its width.
138. **The baseline set's widths come from the layouts.** 085's light
    tables and LiquidType are as wide as 097 says (the same 15, 34, 34,
    9, 3 and 45 as before), so the two can't drift apart.
139. **A table without a row number gets no placeholder.**
    CharacterFacialHairStyles' first column is the race, so a row
    "numbered" by it would mean nothing; asking for one is not found,
    said.

## Decisions (2026-11-05, step 1s, answered here; the owner may overturn any)

140. **The WDT reader is its own part of the reader library.** 098
    takes the file's bytes, as 020's readers do (finding the file is
    the caller's, through the resolver), and fills a struct with no
    pointers, so the editor's LuaJIT FFI reads it as it reads ADTs:
    nothing to free, about 17 KB. 020's `adt_wdt_big_alpha` stays (the
    converter's tests call it); its callers in this client read the
    WDT through 098 instead. 020's MODF entry reading is now its
    `adt_place_read`, shared with 098.
141. **The WDT's list is the map's tiles.** A tile it doesn't list is
    open sea and no file is asked for (the ground, its reading ahead,
    the heights, the run with no window). A tile it lists that no
    place has is open sea too, said on the standard error (the
    window) or the run's log (no window), not refused: a broken
    conversion still starts, and says where. A file a WDT doesn't list
    is not read, as the original client does (recalled).
142. **A WDT with no MPHD or no MAIN is refused.** The ground and the
    heights already stopped without MPHD; a WDT with no tile list is
    not a 3.3.5a one either (the converter always writes it). A version
    other than 18 is read as it is (kept in the struct; unchecked
    against another build). The run with no window, which went on
    without a WDT before, still does: it then asks for each tile's
    file, as before (said when its WDT is there but can't be read).
143. **MAIN's entry k is tile x * 64 + tile y.** As this client numbers
    tiles everywhere (020's AdtWdl, 039, 043), with the file
    <dir>_<tile y>_<tile x>.adt; world-edit-to-execute's W02a writes
    entry ty * 64 + tx for its file <dir>_<tx>_<ty>.adt, the same
    tile. Checked with its own writer on tiles that differ turned
    round, and on five converted maps; the original's own files are
    checked only where an install is (the test program's Kalimdor
    check).
144. **A one-building map is read, not placed.** Its building's name
    and placement are in the struct and 043 names it; placing it (a
    WMO as the whole map) waits for its own step, with 303. Such a map
    is one whose MPHD has 0x1 or that names a building and lists no
    tile (both unchecked: neither project has one).

## Decisions (2026-11-05, step 1t, answered here; the owner may overturn any)

145. **The light tables go through 074 like the rest.** One list of
    Light rows across every place, the map's own place first, so a
    converted map's lights and the install's are both seen; each copy's
    width checked against 097's layout. A band or parameter row no
    place has is a placeholder row where the resolver makes them (said
    and counted by it, as 1q's); where it makes none (or refuses, in
    strict mode) the band is simply absent. Either way a band with no
    moments takes the grey day's value for its slot, so a light with
    half its bands missing still draws. When any of Light, LightParams, LightIntBand or
    LightFloatBand is in no place, the fixed grey day stands everywhere
    and one line names the missing ones: not an error, as a map with no
    lights should still start.
146. **Lights are laid farthest first.** The default first, then the
    lights that reach the point by distance from their centre, farthest
    first, so the nearest ends on top; two within a third of a yard are
    laid larger inner radius first. From 306's clean-room notes on the
    original ("two within 1/3 yard by the smaller inner radius", read
    as the smaller one on top); not run against it. The light the run
    names is the one whose weight is greatest, the last laid on a tie.
147. **A map with no default light takes Light's row 1.** From 306's
    notes; not run against the original. Row 1 is found by its number,
    whatever map it names. With no row 1 either, the grey day; said once
    when the tables are opened, not at each frame.
148. **The run with no window lights at noon.** The window lights by the
    machine's clock (or `--hour`); the run's receipt has to be the same
    twice, so it takes noon unless `--hour` is given, at the point where
    she ends, under the open sky (the clear set; under water is left to
    the window). A test map's own fog is laid over it and held to the
    far clip as 043 does, and `fog_from_map` says so. The receipt counts
    are the window's values (yards, 0-1 colours), not the table's.
149. **No new numbered file.** 026 is rewritten in place; 089, 043 and
    044 gain the rest. `.file-index-counter` stays at 98.
150. **LightSkybox in no place means no skybox.** Said once; the sky
    dome and fog still come from the other four. A skybox row that a
    parameter row names and no place has is no skybox for that light
    (no placeholder row is asked for: it would have no model to name).

## Decisions (2026-11-05, step 1u, answered here; the owner may overturn any)

151. **Colours are cut down to whole 255ths after the time blend.** 306's
    note: "after blending between two times, the original cuts each
    colour channel down to a whole step of 255". Read as truncation on
    the 0-255 scale (with a thousandth's margin for a float that lands
    a hair under a whole step), on each colour band of each set, before
    a storm set or other lights are blended over it; those blends, and
    the fog distances, are not rounded (the note names only the time
    blend and colours).
152. **The fog's floor and range are applied once, to the blended
    light.** The end no nearer than 10 yards; the start's share of the
    end held to -1 to 1, the start then that share of the (floored) end.
    The note doesn't say whether the original applies them to each
    light or to the blend; once, at the end, is the simpler and gives
    the same answer wherever a single light is used. They apply only to
    fog from the light tables: not to this client's grey day (its own
    fixed fog) or a test map's own fog (084). The far clip ending the
    fog is unchanged (084's `map_fog_frame`, 173b6).
153. **The second fog mode: chosen and its start rule kept; its curve
    left.** From map 530 on (306: "maps numbered 530 and above when
    shaders are available"; this client always draws with shaders) a
    negative share counts as 0; below 530 it is kept, so the fog starts
    behind the eye (the shaders' fog clamps, so this draws). The light
    says its mode (`fog_mode`), 043 prints it and the receipt counts it.
    Not built: the end pushed to the far clip when at least 27.78 yards,
    the rate from 1.5 to 7 and its doubling under water, and the curve.
    The notes give no formula for the rate, and say the curve is in the
    original's shipped shader files, which aren't here; pushing the end
    out while drawing the straight-line fog would thin the fog well
    below the original's, so the end stays the table's until the curve
    is read. Both modes are drawn straight-line from start to end.
154. **Storm sets laid over by strength, with nothing reading the
    weather yet.** `light_at_weather` lays each light's storm set
    (Light's field 10; under water 11) over its own set by the storm's
    strength, 0 to 1, before the lights are blended; a light that names
    no storm set keeps its own, and a band the storm set lacks takes
    the grey day's value (as decision 145 for any set). The field
    numbers are 026's layout (recalled, unchecked). The weather comes
    from the server (Phase 5) and nothing here reads it, so 043 and the
    run with no window pass 0 and draw as before; `light_at` is
    `light_at_weather` with 0.
155. **Left, where 306's notes run out.** Zone lights ("up to 5 zone
    lights by percentage") and override lights ("with their own
    fades"): the notes name no table, field or fade rule, so building
    them would be invention. The indoor fog blend (outdoor fog into a
    building group's own over its first 25 yards): it needs the WMO
    groups' fog and their boundaries, which this client doesn't read
    yet (305). The seven-ring dome (note 4.4) is its own step (037).
    The fog's colour as sky colour 6 was already so: the sky's five
    colours are bands 2 to 6 and the fog band 7, the sixth (026's
    layout, recalled).
156. **The test program's output is line buffered.** The parts write
    their warnings to the standard error, unbuffered; with both sent to
    one file, the test program's fully buffered output let a warning
    land inside a check's line, so a count of lines starting "  ok:"
    missed it. Its `main` now sets its standard output line buffered.
    The interleave falls where the buffer happens to fill, so it came
    and went: step 1s's saved runs count the same either way (dd14's
    Master Crafters 564), and step 1t's last dd14 Master Crafters run
    counted 575 by line start against 576 checks; the 576 reported was
    right. No new numbered file: `.file-index-counter` stays at 98.

## Decisions (2026-11-05, step 1v, answered here; the owner may overturn any)

157. **SMSG_WEATHER is read as recalled, and refused when it doesn't
    fit.** This client has no network code yet, docs/004 doesn't list
    the message, and no server source is on this machine, so the layout
    is from memory: opcode 0x2F4, a body of 9 bytes (a 4-byte state, a
    float grade, a 1-byte abrupt flag), no sound id (older clients'
    messages carried one; 3.3.5a's, as recalled, doesn't). A body of
    any other length is refused, naming its length, so a wrong guess
    shows on the first real message instead of being read as nonsense;
    a grade that isn't a number, or an abrupt byte other than 0 or 1, is
    refused too; a grade outside 0 to 1 is held to it. The state numbers
    in 099-weather.h are the server's, recalled.
158. **Every state but fine is a storm of its grade, eased over ten
    seconds.** The notes say only that storm sets are laid over by the
    storm's strength; which states count (fog, rain, snow, sandstorm)
    and how a grade maps to strength are not in them, so every state
    but fine counts and the grade is the strength. A change eases in a
    straight line over ten seconds from wherever the strength stands
    (so a change half way through starts from there), or is there at
    once when abrupt. The original's easing time is unknown.
159. **The weather comes from `--weather` or the moves file until the
    client talks to a server.** 043's `--weather STATE GRADE` sets it from
    the start, at once, for the window (eased on the world's seconds,
    the frames' capped steps) and the run with no window. A move may
    carry `weather = { state, grade, abrupt }`, taken as the move
    begins on the steps' clock, so a test can drive a change and its
    easing; a weather alone takes no steps. The moves format stays 1:
    the field is new, and every older file reads as before. The receipt
    counts the last weather's state and grade, how many were taken, and
    the storm's strength where she ends (the light there uses it).
160. **The weather is its own file, 099.** Window-free (CORE), not in
    the reader library (the editor has no use for it). The counter goes
    to 99.
161. **A building's fogs are read as recalled.** MFOG, 48 bytes a fog
    (flags, a centre, two radii, then an end, a start share and a
    colour, in and under water); a group's four fog numbers at MOGP +
    48, its bounding box at MOGP + 12. Only the fog numbers' place has
    any support here: the four bytes right before the liquid field at
    MOGP + 52 that 173r reads. Colours are blue-green-red bytes, as
    MOCV's; distances are taken as yards, as the building's points are.
    WmoRoot and WmoGroup gained their fields at the end (the editor's
    FFI header is written from these headers by scripts/build).
162. **The indoor blend is a straight line over 25 yards, to a box.**
    306: "outdoor fog blends into a building group's own fog over the
    first 25 yards past its boundary (its underwater fog in liquid)".
    `light_indoor_fog` mixes colour, start (the share times the end) and
    end in a straight line over those 25 yards; none of the original's
    other fog rules (the floor, the modes) are applied to a building's
    fog, the notes not saying. How far in a point is, is measured to
    the group's bounding box, standing in for the group's own shape.
163. **Where the notes run out for buildings.** Which of a group's four
    fogs applies at a point (each fog has a sphere with two radii, most
    likely picked or blended as lights are, but the notes don't say),
    and whether the eye is inside a group at all (the box is a stand-in;
    305's portal walk is not built), are left. So nothing calls the
    blend in the window or the run with no window yet. The converter
    writes no buildings (WC3 buildings become WC3 models), so there is
    no converter build to check this on.
164. **"No row 1" is said for the map being lit.** Light's row 1 is only
    a fallback for a map with no default light of its own, so opening
    the tables no longer says it has none: `light_map_default` gives the
    light under every sphere on a map (its own default, else row 1, else
    none) and says so only in the last case. 043 and the run with no
    window call it once for their map. A map converted with #260 has its
    own default, so its log says nothing of row 1.
165. **The checks on a build take its own light or the grey day.** A
    build converted before world-edit-to-execute's #260 has no light
    rows: the grey day, as before. One converted with it has the map's
    own default light: the checks then want that light's id, read from
    the map's folder's rows (its copy first), no placeholder row made,
    the run with no window lit by it, and with no baseline set still lit
    by it. Which a build is, is read from whether its folder has
    DBFilesClient/Light.dbc. The id (20000 on these maps) isn't fixed in
    the checks: it is whatever the map's own default is.

166. **The layouts are held to the converter's by its own written
    bytes.** A Lua check by the repo's "matches Lua" pattern (050, 062,
    064: a project folder, `--build`, `--wete`), not a comparison of
    the two lists' texts: the converter's own `dbc.write` makes the
    file, this client's own 016 and 097 read it, so what is compared is
    what one writes and the other reads. It skips, saying so, when no
    world-edit-to-execute tree is given, since the tree is outside this
    repo (an exported commit is enough). A disagreement is a failure;
    a column this client reads that the converter's `seen` doesn't
    list is a note (decision 170).
167. **097 moves into the reader library.** The check reads the layouts
    through `libwreaders.so` from LuaJIT, as 050, 062 and 064 read
    their parts; 097 needs only 013 and keeps no state, so it belongs
    with the readers, and any Lua caller can ask it the same way. CORE
    takes it through READERS as before; nothing else changes.
168. **Light's sets 3 to 5 are read (the converter was right).** 026
    reads all five of a light's parameter sets (fields 8 to 12) since
    step 1u, but 097 still marked 10 to 12 unchecked; #260's `seen`
    lists all five. 097 now marks them read by 026, so 074 refuses a
    Light copy narrower than 12 fields where it refused one narrower
    than 9. The fifth set (field 12) is read and not yet used; that it
    is the ghost world's is recalled. Every Light this client has met
    (the baseline set's, #260's, the test program's) is 15 wide.
169. **The nine tables the converter doesn't write stay unchecked in
    width.** CreatureDisplayInfoExtra, CharStartOutfit,
    ItemDisplayInfo, CharHairGeosets, CharacterFacialHairStyles,
    CharSections, HelmetGeosetVisData, LiquidType and LightSkybox: the
    converter has no layout for them, so nothing on its side agrees or
    disagrees. Their read columns stay checked from this client's
    readers, their widths as `width_from` says.
170. **The converter's `seen` gaps are notes, for its side.** Its `seen`
    omits ID (field 1) of AreaTable, Faction, FactionTemplate and
    LoadingScreens, which 074 reads to find a row, and LightIntBand's
    and LightFloatBand's Time2 to Time15 and Data2 to Data15, which 026
    reads (it lists only Num, Time1 and Data1). Nothing here is wrong
    for it: `seen` is the converter's account of this client and lags
    it. Printed by the check, not failed, and left to world-edit-to-execute.
171. **The file counter is at 100** (`tests/100-layouts-match-converter.lua`).

172. **A check that can't be made here is NEEDS, not ok and not FAIL.**
    When a check reads what only the owner's machine has (the client's
    install, the server's extracted files, war3.mpq) and it isn't there,
    the test program prints `NEEDS:` with the check and what is missing.
    It counts these apart from the checks made and names the count in
    the closing line. It exits 2 when nothing failed but something was
    NEEDS, 1 when anything failed, 0 only when every check was made and
    passed. So a run missing data is never mistaken for a clean one. Only
    absence counts: a folder or file that is there and wrong is still a
    FAIL. The parenthesised "skipped" notes for a missing converted
    build are left as they were: a build is made per run, not owned.
173. **No install no longer ends the run.** The archives' part was the
    only one that returned: its FAIL went uncounted, every later check
    went unrun and the closing line went unprinted. Now the archives' part and
    the six later parts that open the install are each one NEEDS when
    the folder holds no archives (`client_open`); archives that are
    there but won't open are a FAIL. WC3's pictures, which need only
    war3.mpq, moved before the archives.
174. **The made-up slopes take 60 degrees without the server's tiles.**
    They check the mover against a limit; that the server's limit is 60
    is the check before them. Without the tiles they took 019's -1, so
    two failed and two passed for the wrong reason. With the tiles they
    take the server's value, as before, so on the owner's machine
    nothing changes.
175. **The tile seam check needs points compared.** With no map files
    it compared none and passed. It is NEEDS then, and with them it
    must compare at least one point.
176. **war3.mpq absent is NEEDS.** The footman picture's checks need
    world-edit-to-execute's copy of WC3's install. It isn't here, so its
    four checks are one NEEDS; a war3.mpq that is there must open and
    decode as before.

177. **The loading screen picture is the slice after 1x.** Of the
    work that needs no window, install or server, it reads what the
    converter writes and the client ignored (LoadingScreens and the
    picture). Every part of it can be checked against code or data on
    both sides: the converter's own writer, the 1w-checked layouts and
    the real builds. 303's one-building map needs data the converter
    doesn't write, and this machine has none.
178. **Columns by name.** `table_layout_find` (097, in the reader
    library) gives a column's place from its name. 101 asks for
    "LoadingScreenID" rather than 58, so it follows the layouts the
    converter is held to.
179. **No picture is no loading screen.** 101 reads the picture with
    asset_read, as an interface picture, which never gives a
    placeholder. A missing or broken picture leaves the window dark, said
    in a line, rather than drawing the magenta checker across it.
180. **LoadingScreenID doesn't make Map refusable.** 097's read marks
    are what 074 refuses a narrower copy by. A Map copy too narrow for
    field 58 would then be refused whole, map and all, for want of an
    optional picture (074's own fixtures are 3 fields wide). So the column
    stays unmarked and 101 checks the copy's width itself. LoadingScreens'
    four columns are marked read by 101: the whole table, its width
    already the converter's.
181. **HasWideScreen is read and kept, nothing more.** What the original
    does with it (a wide copy of the picture) is recalled; the
    converter writes 0.
182. **The picture's file is held to the converter's writer.** A Lua
    check in the matches-converter pattern of 100. The converter's
    `loadscreen.blp2` writes the file, this client's 017 reads every
    copy back, and each is compared with the converter's own resample
    of that size. That is the converter's code, not memory of BLP2's
    layout. The builds' real files add that each smaller copy is the one
    before halved. The counter is at 102.

## Open questions

1. ~~**Test-play: who runs the rules?**~~ Answered 2026-10-09: always a
   real AzerothCore running on the editor's machine, option (b) below (the
   owner chose it over a rules-keeper inside the client). Test-play behaves
   exactly as players will get it, and needs a local server to run at all;
   the client keeps no rules of its own, as with everything else ("World
   changes come only from the server", CLAUDE.md). The question as asked:
   The editor's F5 today runs
   world-edit-to-execute's Lua simulation in the viewer. Under the
   decision that AzerothCore runs a map's rules, test-play could (a)
   run that Lua simulation locally as the offline stand-in for the
   server (the role 515 gave "the local simulation" offline), or (b)
   convert the map and play it on a local AzerothCore. (a) works now and
   without a server; (b) tests what players will really get.
   Partly answered 2026-10-09, the owner: "the editor will converge with
   the custom client's engine, so we aren't maintaining more than one copy
   of the same engine." So there is one engine: the editor's drawing and
   running move onto this client's, and world-edit-to-execute's viewer
   stops being a second copy. Then asked: within that one engine, does
   test-play's rules-keeping (who moves units, resolves attacks, counts
   resources) run inside the client as the offline stand-in, which (a)
   becomes, or always on a local AzerothCore, which (b) is? (b), as above.
   The 2026-11-04 decision keeps this answer: Test Map plays on a local
   AzerothCore. It changes the "one engine" part only for drawing: the
   editor keeps its own Lua renderer for editing.
2. ~~**The editor's drawing calls.**~~ Answered 2026-10-09, the owner: "We
   can push the editor to phase 7. It's important that WoW addons work
   as-is." So the editor waits for Phase 7 and is rebuilt on its
   interface runtime; its panels are made the way addon authors make
   WoW's interface, and that runtime runs original addons unchanged
   (docs/006). The question as asked: Offer the small surface the editor
   calls today (about a dozen calls: interface text, rectangles, frames,
   images, models, ground, designs) as-is, so the editor moves
   unchanged; or move the editor onto Phase 7's interface runtime (XML
   frames and Lua, as WoW's interface is built) once it exists?
   Withdrawn 2026-11-04: the editor no longer runs in this client, so it
   is not rebuilt on Phase 7's runtime; it keeps world-edit-to-execute's
   drawing surface.
3. ~~**Who starts the local AzerothCore?**~~ Answered 2026-11-04, the
   owner: "the editor." The editor starts it, reuses it across test runs
   and stops it when the editor closes (world-edit-to-execute issue
   913); this client is handed a running server and a map. The question
   as asked: the editor (it knows the map and the converted folder),
   this client (it knows when it needs a server), or a small shared
   launcher both call?

4. ~~**Which map a test map is.**~~ Answered 2026-11-04 (decision 12):
   from the build folder's `map.lua`, which world-edit-to-execute's W02c
   writes; `--map` isn't taken with `--test-map`. The question as asked:
   The converted map registers itself (W02: its Map.dbc row, its folder
   of ground tiles), but the client still opens the map `--map` names,
   and reads ground from the archives only. When 302 reads WC3 ground
   and the readers ask the resolver, should the client take the map's
   id from a file in the build folder (which W02 would write, say
   `map.lua`), or from a `--map` the editor passes?
5. **Being stopped.** When the editor closes while the client runs (913:
   it stops its server then), or the player presses Ctrl-C in a
   terminal, the client is killed and leaves no receipt, which reads as
   a crash. Catching the stop signal and ending with a receipt (reason
   `closed`, or a new `stopped`) needs a flag the signal sets, state
   outside any handle (docs/001, "Handles"). Worth the exception?
6. **A WC3 model 061 refuses.** War in the Plaguelands'
   `war3mapImported\glow.mdx` ends inside its last solid shape (a
   cylinder, CLID shape 3, holding two numbers where 061 reads more),
   so 061 refuses the whole model and it is left out (decision 30).
   world-edit-to-execute's Lua reader reads it the same way. Should
   both readers take a short last shape (the model drawn, its shape
   dropped), or is the file broken and refusing it right?
7. **Buildings from a real conversion.** No stock row W02b converts is
   a building (WMO) yet, so neither real conversion has a MODF entry:
   buildings are checked only with files made in the test. The first
   map that places one is the check.
8. **The corner bits on real tilesets.** 063 and the Lua read a
   subtile's number as south-east 1, south-west 2, north-east 4,
   north-west 8 (302a's open item); 078 lays the subtile with north at
   the top of the picture. Checked only on made pictures drawn by the
   same rule: on the owner's machine with WC3, a converted map's
   blends mirrored would mean that one table changes.
9. **WC3's cliff models, unseen here.** Their names (decision 43),
   that they lie one cell west of their origin (decision 46), which
   variations exist, and `CliffTypes.slk`'s columns are from the
   published map viewers' reading, checked only against models and
   sheets made in the test. On the owner's machine with WC3: a Test Map
   run of War in the Plaguelands showing its cliffs a cell off, or
   boxes where cliffs should be, says which rule to change.
10. **WC3's ramp models and blight, unseen here.** The ramp pieces'
    letters (`L`, `H`), that one covers the ramp's flat half, and that
    the blight is `WorldEditData.txt`'s second field are the viewers'
    reading from memory. On the owner's machine: Master Crafters' ramps
    as boxes, or doubled, or its blight the checker, says which.
11. **WC3's tileset pictures, unseen here.** That subtiles 0 and 15 of a
    plain picture are both whole tiles, which of them variation 0 picks
    (decision 60), and that an extended picture's right half is 16
    whole variations are the viewers' reading; checked only against
    pictures made in the test. On the owner's machine with WC3: a Test
    Map run showing see-through cells, or every cell alike, says which.
12. **WC3's water, unseen here.** The sheet's row name (the tileset's
    letter and `Sha`), its columns (`texFile`, `numTex`, `texRate`,
    `height`, the `Smin_`, `Smax_`, `Dmin_`, `Dmax_` colours, alpha
    first), the frames' names (two-digit numbers from 00) and the
    colour rule's depths (10, 64 and 72 over 128 of a cell) are
    HiveWE's reading, recalled rather than read here; checked only
    against a sheet made in the test. On the owner's machine with WC3:
    081 refusing the install's sheet, or a frame not found, says which.
    (Since step 1i also: the frame repeating once a cell, and laid
    columns east and rows south as the game's water is, decision 71.)
13. **The map's water tint and fog, unseen here.** Which flag turns the
    tint on (0x10000, the published notes' and HiveWE's reading as
    recalled), which turns the fog on and what style 0 means (0x2000
    and linear, decision 76; the notes and world-edit-to-execute: style
    0 none), and both colours' byte order (red first by the notes; the
    Lua reader names the fog's bytes blue first). War in the
    Plaguelands reads as a cyan tint (50, 255, 255) and a blue fog (30,
    120, 255) red first, a yellow tint and an orange fog blue first. On
    the owner's machine with WC3: its water's colour and its haze in
    WC3 beside a Test Map run say which.
    What the repositories hold (since step 1j; world-edit-to-execute
    read, not changed): its `docs/formats/w3i-map-info.md` gives fog
    style 0 as none, flags only to 0x8000 (0x2000 to 0x8000 "purpose
    unknown", nothing past it), and both colours as "BGRA", the fog's
    as "(BBGGRR00)", which puts its alpha where neither its own writer
    (fog 0xFF000000, tint 0xFFFFFFFF: alpha the fourth byte) nor any
    map here does; `src/parsers/w3i.lua` reads the fog's colour as one
    number and takes its third byte as red (as WC3's region colours,
    `war3map.w3r`, really are stored blue first, and its w3r reader
    reads them so), and keeps the tint as one number, unsplit;
    `src/tests/test_w3i.lua` checks neither colour, nor the fog or
    flags past being present. The 18 maps read (decision 76): 0x8000
    is set on all 18 (16 of them with the default white tint and no
    0x10000), 0x10000 on two (War in the Plaguelands, cyan, and DAoW
    1.23.1B, white), consistent with 0x10000 for the tint; two maps' magenta
    fog (255, 0, 255) reads the same either way. So the repositories
    don't settle the byte order: red first is kept.
    Follow-ups for world-edit-to-execute (not changed here; for the
    coordinator): (a) its reader, at versions 28 and later, which it
    accepts, doesn't pass the game's version (four i32 after the
    editor's version) or the script-language number after the tint (and
    version 31's two more), so every field after the editor's version
    is misread for those maps; none of the 18 maps read here is past
    25, so no test meets it. (b) Its fog colour's red, green and blue are
    probably the file's first three bytes (decision 72), not its third,
    second and first. (c) Its fog style 0 and flag 0x2000 per decision
    76, and its flags table (0x2000 terrain fog, 0x10000 water tint)
    in the reader and the format notes. (d) Its test checks no fog,
    tint or flag values.

## Related

- Blocked by: [401](./401-m2-reader.md) (step 2, WC3 models),
  [302](./302-terrain-renderer.md) (WC3 ground on the one mesh),
  [105a](./105a-loose-overlays-and-asset-resolver.md) (the converted map
  as an overlay; wire-frames), world-edit-to-execute's W02 (conversion).
- Built here for step 1: `src/067-test-map.info.md` (the launch),
  `src/068-goodbye.info.md` (the receipt); stacked on 105a's resolver.
  For step 1b: `src/073-converted-map.info.md` (`map.lua`),
  `src/074-tables.info.md` (tables per row),
  `src/075-adt-heights.info.md` (heights from the ADTs).
  For step 1c: `src/076-placed-models.info.md` (a placed model's
  files), `src/042-doodads.info.md`, `src/060-resolver.info.md`.
  For step 1d: `src/077-slk.info.md` (WC3's sheets),
  `src/078-wc3-ground.info.md` (the ground in WC3's tilesets),
  `src/063-w3e.info.md` ([302a](./302a-wc3-ground-read-and-prepared.md)).
  For step 1e: `src/079-wc3-cliffs.info.md` (the cliffs), with 078,
  076 and 077. For step 1f: 079 (ramps) and 078 (blight). For step 1g:
  `src/080-ground-shader.info.md` (the ground's shader), with 078
  (each cell's pieces), 033 (packed for the card) and 038. For step
  1h: `src/079-wc3-cliffs.info.md` (each cliff's bend),
  `src/076-placed-models.info.md` (a model bent by its name),
  `src/081-wc3-water.info.md` (WC3's water read). For step 1i:
  `src/082-water-prep.info.md` (the water's points and shader),
  `src/083-w3i.info.md` (the map's settings), `src/040-water.info.md`
  and 081. For step 1j: `src/084-map-fog.info.md` (the map's fog), with
  083, 026 and 059. For step 1k: `src/085-baseline-set.info.md` (the
  smallest set), `src/086-make-baseline.info.md` (its program),
  `src/060-resolver.info.md`, 014 and 040. For step 1l:
  `src/087-world-start.info.md` (the start out of the window),
  `src/088-figure-model.info.md` (which files a figure is made from),
  with 085, 041, 074, 026, 040 and 043. For step 1m:
  `src/025-character.info.md` and `src/034-prefetch.info.md`. For step
  1n: `src/089-headless-run.info.md` (the headless run and its moves),
  `src/068-goodbye.info.md` (the receipt's counts), 067, 087, and
  `tests/090-test-map-moves.lua`. For step 1o:
  `src/091-model-solids.info.md` (the models' floors and walls with no
  window), 089, 042, 043, 087, and `tests/092-test-map-moves-plaguelands.lua`
  and `tests/093-test-map-moves-master-crafters.lua`. For step 1p:
  `src/094-wc3-pathing.info.md` (the map's WC3 pathing as walls), 091,
  089, 043, 068, and `tests/095-test-map-walls-plaguelands.lua` and
  `tests/096-test-map-walls-master-crafters.lua`. For step 1q:
  `src/074-tables.info.md` (a search's rows, placeholder rows), 025,
  060, 088, 089 and 043. For step 1r: `src/097-table-layouts.info.md`
  (the tables' layouts), 074, 026, 041 and 085;
  [106](./106-dbc-parser.md) (layouts). For step 1s:
  `src/098-wdt.info.md` (a map's tile list), 020, 033, 034, 075, 089
  and 043; [303](./303-wdt-and-streaming.md) (the WDT parser). For
  step 1t: `src/026-light.info.md` (the light through the tables), 074,
  097, 089, 043 and 084; [306](./306-light-and-sky.md) (the light and
  sky, and the original's notes). For step 1u: 026 (the original's
  colour, fog and storm rules), 084, 089, 043 and 044's `main`. For
  step 1v: `src/099-weather.info.md` (the weather's storm), 026, 021,
  089 and 043; [305](./305-wmo-renderer.md) (a building's fogs). For
  step 1w: `tests/100-layouts-match-converter.lua` (the layouts
  held to the converter's), `src/097-table-layouts.info.md`, 016, 026
  and 044; [106](./106-dbc-parser.md) (layouts); world-edit-to-execute's
  draft PR #260 (commit 306b40f40, its light tables). For step 1x:
  `tests/044-test-client.c` (NEEDS, its closing line and exit status)
  and [008](../docs/008-build-system.md) (the tests). For step 1y:
  `src/101-loading-screen.info.md` (the loading screen), 097, 017, 043,
  089, `tests/102-loading-screen-matches-converter.lua`, and
  [606](./606-the-loading-screen.md) (the loading screen's picture;
  its bar and the rest not built).
- world-edit-to-execute's W02c (map registration and output forms,
  draft PR #243 there): writes the build folder read here.
- world-edit-to-execute's draft PR #252 (branch
  claude/wete-w02-wpm-copy): copies the map's `war3map.wpm` into the
  build folder, read by step 1p.
- [1108](./1108-phase-demo.md) (its 1101 camera, 1102 picking, 1103
  selection, 1104 orders are what Test Map plays with).
- world-edit-to-execute issues 901-912 (the editor), its
  `docs/wow-client-bridge.md` ("2026-11-04: the editor keeps its own
  renderer").
