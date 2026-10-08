# 1109 — The Editor as a Mode of the W Client

*Decided 2026-10-07. The owner: "let's make the editor a mode of the W
client." Came from: world-edit-to-execute's map editor (its Phase 9,
issues 901-912), which today runs on that project's own renderer.*

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

This client has no editor mode, reads no WC3 ground or WC3 models yet
(pictures, yes: 107a), and has no interface runtime for Lua (Phase 7).

## Intended behavior

The editor runs **in this client, as a mode** beside the WoW world and
WC3 map mode. Its code stays in world-edit-to-execute (that project
keeps the editor: "so we can keep working on things like the editor
here"); this client loads it as the mode's Lua and gives it everything
it draws and hears with. So there is one renderer for both projects,
and world-edit-to-execute's renderer can retire.

What the mode needs from this client:

- **WC3 maps read directly**: the ground (`war3map.w3e`) painted onto the
  one ground mesh with WC3's tilesets and cliffs seated on it (docs/009,
  answer 1), drawn again after each edit; doodads and units with their
  models (401 step 2, MDX) and the flat-shaded designs as placeholders
  (docs/009, answer 2); the map's own imported art first.
- **A drawing surface for Lua**: the 2D interface pieces the editor uses
  today, and the editor's panels (object, trigger, sound, import, AI,
  region and camera editors).
- **Picking**: the ground point and the object under the cursor
  (1108's 1102), box select (1103).
- **The top-down camera** (1108's 1101) with the editor's pan and zoom.
- **Sound** for the sound editor (Phase 10).
- **Test-play** of the map being edited (open question 1).

Order: world-edit-to-execute's renderer is retired only after this mode
opens, edits, saves and test-plays a map, so the editor never stops
working in between.

## Suggested implementation steps

1. The mode's frame: open a `.w3x` from a path, the top-down camera,
   the editor's Lua loaded from world-edit-to-execute's `src/editor/`.
2. WC3 ground on the one mesh, redrawn by tile after a stroke.
3. Models (after 401 step 2) and the designs as placeholders.
4. The Lua drawing surface (open question 2), then each panel in turn.
5. Picking and selection; saving through the editor's own writer.
6. Test-play (open question 1).
7. Check against the editor running in world-edit-to-execute's viewer
   (the same map, the same edits, the same saved bytes), then retire
   that viewer there.

## Acceptance

- The editor opens Daow 4.4 in this client, paints ground, places and
  moves a unit, edits a trigger, saves, and the saved map is byte for
  byte what world-edit-to-execute's viewer saves for the same edits.
- F5 test-plays the edited map.

## Open questions

1. **Test-play: who runs the rules?** The editor's F5 today runs
   world-edit-to-execute's Lua simulation in the viewer. Under the
   decision that AzerothCore runs a map's rules, test-play could (a)
   run that Lua simulation locally as the offline stand-in for the
   server (the role 515 gave "the local simulation" offline), or (b)
   convert the map and play it on a local AzerothCore. (a) works now and
   without a server; (b) tests what players will really get.
2. **The editor's drawing calls.** Offer the small surface the editor
   calls today (about a dozen calls: interface text, rectangles, frames,
   images, models, ground, designs) as-is, so the editor moves
   unchanged; or move the editor onto Phase 7's interface runtime (XML
   frames and Lua, as WoW's interface is built) once it exists?

## Related

- Blocked by: [401](./401-m2-reader.md) (step 2, WC3 models),
  [302](./302-terrain-renderer.md) (WC3 ground on the one mesh), Phase 7
  for the interface runtime if open question 2 says so.
- [1108](./1108-phase-demo.md) (its 1101 camera, 1102 picking, 1103
  selection are shared with this mode).
- world-edit-to-execute issues 901-912 (the editor), its
  `docs/wow-client-bridge.md`.
