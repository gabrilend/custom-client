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

Not built yet: the map's ground drawn with WC3's tilesets (302: the
ADTs are drawn as any ADT, their pictures from the archives); a WC3
model posed and animated (it stands in its rest pose; 402 step 4, PR
#7) or team-coloured (404: its replaceable pictures are left off); a
missing table row as a placeholder (106 wants one: still an error
naming the table and row); the resolver for pictures, doodads and
buildings (204, 302); logging in, and the `lost-connection` ending
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
2. The editor's side (in world-edit-to-execute): F5 saves, converts,
   starts or reuses the local AzerothCore, launches this client with
   the option, and waits for the goodbye receipt.
3. WC3 ground on the one mesh (302), then models (after 401 step 2).
4. The RTS controls from 1101-1104.
5. Check: the same map test-played twice gives the same receipt counts
   for the same scripted input.

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
- world-edit-to-execute's W02c (map registration and output forms,
  draft PR #243 there): writes the build folder read here.
- [1108](./1108-phase-demo.md) (its 1101 camera, 1102 picking, 1103
  selection, 1104 orders are what Test Map plays with).
- world-edit-to-execute issues 901-912 (the editor), its
  `docs/wow-client-bridge.md` ("2026-11-04: the editor keeps its own
  renderer").
