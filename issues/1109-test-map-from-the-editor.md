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

This client has no way to be handed a map by the editor and play it.
It draws no WC3 ground and reads no WC3 models yet (pictures, yes:
107a). WC3 ground is read and prepared since 2026-11-04
([302a](./302a-wc3-ground-read-and-prepared.md)); drawing it on the
one mesh is 302's.

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

## Suggested implementation steps

1. The launch option: open a converted map folder as an overlay, skip
   the login and character screens, place the camera.
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

## Related

- Blocked by: [401](./401-m2-reader.md) (step 2, WC3 models),
  [302](./302-terrain-renderer.md) (WC3 ground on the one mesh),
  [105a](./105a-loose-overlays-and-asset-resolver.md) (the converted map
  as an overlay; wire-frames), world-edit-to-execute's W02 (conversion).
- [1108](./1108-phase-demo.md) (its 1101 camera, 1102 picking, 1103
  selection, 1104 orders are what Test Map plays with).
- world-edit-to-execute issues 901-912 (the editor), its
  `docs/wow-client-bridge.md` ("2026-11-04: the editor keeps its own
  renderer").
