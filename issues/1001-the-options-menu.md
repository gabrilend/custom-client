# 1001 — The Options Menu

*Came from: wow-chat-2026 173b and its sixteen panels (173b1 to 173b16),
moved 2026-10-07. The panels' own issues there hold each setting's
detail; this issue carries them on.*

## Current behavior

- **The settings store** (`src/014-settings.c`, a handle): every setting
  declared once (name, kind, range, default, section, whether it needs a
  restart), the original's names; the player's `Config.wtf` read
  (bad lines named by number and skipped) and written; read by name each
  time, so a change takes effect at once. More than one store can live
  at once (a profile per server).
- **Declared and working:** the Camera and Mouse panels' settings with
  the camera (207), View Distance, Particle Density and Texture
  Filtering (Effects), Resolution, Windowed Mode, Maximized, Vertical
  Sync and Disable Resize (Resolution), the window's size kept when the
  player resizes it.
- **Not built:** the menu itself (the original's three settings
  windows, Escape's game menu); key bindings (built with the action bar:
  the owner, 2026-10-06); every other panel, each with its feature.
- The original's whole menu, every panel and setting, is listed in
  docs/048 (generated from its own interface files).

## Intended behavior

The client offers every setting the original's menu offers, plus the
project's own. Each setting is built together with the feature it
controls; the menu draws itself from the store, so a newly declared
setting appears in its panel without screen code. One table of panels:

| Panel (wow-chat-2026) | What | Built with |
|---|---|---|
| 173b2 the options screen | Escape's game menu and the three windows, from the store | the interface (Phase 7) |
| 173b3 key bindings | every action by name, two keys each, per character over a global set | the action bar |
| 173b4 camera and mouse | partly built | 207 |
| 173b5 video: resolution | partly built; gamma open | — |
| 173b6 video: effects | view distance, particles, filtering built | each effect's feature |
| 173b7 sound | volumes, channels | Phase 10 |
| 173b8 controls and combat | auto loot, self-cast ... | Phase 8 |
| 173b9 display and objectives | minimap, threat, quest tracking | Phase 7 |
| 173b10 action bars | extra bars, lock | the action bar |
| 173b11 names and nameplates | whose names show | Phase 8 |
| 173b12 combat text | floating combat text | Phase 8 |
| 173b13 status text and unit frames | numbers on bars, party frames | Phase 7 |
| 173b14 buffs and debuffs | durations, markings | Phase 8 |
| 173b15 social and chat | bubbles, filters | Phase 9 |
| 173b16 features, help, languages | tooltips, tips, language | Phase 7 |

### Changes wait for Apply, and each one is previewed first

The owner, 2026-10-20: "we should have a list of changes that are
applied when the user hits 'apply' or whatever. One of these might
unload and re-load all the models in the game. We should have a display
next to the option that allows the user to see the effect of their
changes in real-time before they hit apply."

- **The waiting list.** Changing a setting in the menu doesn't change
  the store. It adds a line to a list of waiting changes, shown in the
  menu: the setting, its value now, its new value, and what applying it
  will do. Changing the same setting again replaces its line; changing
  it back to its value now removes the line. **Apply** carries out the
  whole list, then writes the store and `Config.wtf`.
- **Each setting declares its apply action**, beside its name, kind and
  default in the store, chosen from a short table of actions: nothing
  beyond reading the new value; rebuild the textures; unload and reload
  every model; rebuild the shadow maps; recreate the window; only after
  a restart. Apply runs each action once, however many waiting changes
  need it, in that table's order. The list shows the cost before the
  player commits to it, for example "reloads every model".
- **A preview beside the setting** shows its effect live while the
  change is still waiting. It's a small view drawn by the client's own
  renderer with the waiting value, not a picture of the effect, so what
  it shows is what Apply will give. Each preview scene is described as
  data (which model, which ground, where the camera and the sun are) and
  drawn by one preview viewer, keeping the scenes apart from the
  drawing:
  - **Model detail:** one model, playing an animation, which the player
    can turn with the mouse, with a pause button just below it. The
    model is chosen by a tool that searches the game's archives for the
    model whose four detail files differ most, so every bone limit
    looks different.
  - **Texture size:** the same textures shown at each size the setting
    offers, the waiting one marked.
  - **Shadows:** a little piece of ground with a house on it, the camera
    on the house's shadow. Moving a shadow setting redraws the scene at
    once.
  - Other settings get previews the same way as their features are
    built. A setting with no preview yet says so beside it.

## Suggested implementation steps

1. The menu screen, drawn from the store (with the interface, Phase 7).
2. Each panel with its feature, as the table says.
3. Every setting the original registers, menu or not (docs/057
   Appendix A, filled 2026-10-16: 435 names with their defaults and the
   original's own help text), declared in the store by a generator that
   reads that table, as `scripts/generate-stock-options-catalog` builds
   docs/048 from the interface files. Declared with its name, default
   and help text even before its feature exists, so a `Config.wtf` from
   the original reads without a warning for each setting this client
   doesn't use yet, and the console's help shows the original's text.
   A setting whose feature isn't built is marked so, and says so when
   changed. The few whose default is computed at start-up (camera view
   and smoothing style, mouse speed, sound effects on one-processor
   machines) get that rule, not a number.
4. The waiting list and Apply: each setting's apply action declared in
   the store, the list drawn in the menu, Apply running each action
   once. Tests: changing view distance twice and then back leaves the
   list empty; two waiting model-detail changes reload the models once;
   closing the menu leaves the store unchanged until Apply.
5. The preview viewer, which draws a preview scene (data) with waiting
   values in a small view; then the three first scenes: the turning
   model with its pause button (needs [401](./401-m2-reader.md) and
   [402](./402-skeletons-and-animation.md)), the textures at each size,
   and the ground with a house for shadows (needs
   [310](./310-shadows-and-live-light.md)). The tool that picks the
   model whose detail files differ most. Test: the model-detail preview
   at 21 bones draws a different file from the one at 256, while the
   world keeps its current one.
6. The project's own settings, which the original doesn't have, each
   marked as the project's in its panel so a player can tell them from
   the original's:
   - **Model detail** (Effects panel; [401](./401-m2-reader.md),
     answered question 1): the bone limit a model's detail file is
     chosen by, one of 256, 64, 53 or 21, default 64 as on a typical
     player's machine. Its apply action reloads every model; its
   - **Selection colours** (Names and nameplates panel;
     [809](./809-targeting-and-range.md), answered question 1): the
     original's plainer colour variant by default (until docs/057 says
     which variant the original uses where), with its fuller variant,
     the one with party, friend and dead colours, as a switch.

## Open questions

1. **Gamma's curve** (from wow-chat-2026 173b5). The original's Gamma
   slider (-0.5 to 0.5) changes the display's gamma ramp; this client
   would do it in its shaders. Should the slider raise each colour to the
   power 1 / (1 + gamma), or be matched by eye against the original at a
   few positions?

2. **Closing the menu with changes still waiting.** Throw them away,
   apply them, or ask? Asked 2026-10-20.

3. **Which panels wait for Apply.** The waiting list as written covers
   every panel, so even mouse speed waits. A camera setting is easiest
   judged by moving the camera with it. Should cheap settings whose
   apply action is "nothing" take effect at once, or should every
   panel wait for Apply? Asked 2026-10-20.

## Related

- docs/048 (the original's menu), [207](./207-camera.md), [310](./310-shadows-and-live-light.md),
  [401](./401-m2-reader.md), [809](./809-targeting-and-range.md).
