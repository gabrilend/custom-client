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

Not built yet: drawing anything from the map's folder (the readers
still ask the archives directly; 302, 401 and 204 move them to the
resolver), choosing the map by the converted map's own registration
(open question 4), logging in, and the `lost-connection` ending (Phase
5's 609). The client reads no WC3 ground or WC3 models yet (pictures,
yes: 107a).

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
   ("A clean ending"). No screens exist to skip yet; the map's folder
   is in the resolver, which no reader asks yet; the map is the one
   `--map` names (Kalimdor by default) until the converted map says
   its own (open question 4).
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

4. **Which map a test map is.** The converted map registers itself
   (W02: its Map.dbc row, its folder of ground tiles), but the client
   still opens the map `--map` names, and reads ground from the archives
   only. When 302 reads WC3 ground and the readers ask the resolver,
   should the client take the map's id from a file in the build folder
   (which W02 would write, say `map.lua`), or from a `--map` the editor
   passes? Until answered: `--map`, defaulting to Kalimdor.
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
- [1108](./1108-phase-demo.md) (its 1101 camera, 1102 picking, 1103
  selection, 1104 orders are what Test Map plays with).
- world-edit-to-execute issues 901-912 (the editor), its
  `docs/wow-client-bridge.md` ("2026-11-04: the editor keeps its own
  renderer").
