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
4. The project's own settings, which the original doesn't have, each
   marked as the project's in its panel so a player can tell them from
   the original's:
   - **Model detail** (Effects panel; [401](./401-m2-reader.md),
     answered question 1): the bone limit a model's detail file is
     chosen by, one of 256, 64, 53 or 21, default 64 as on a typical
     player's machine. Applies to models loaded after the change.
   - **Selection colours** (Names and nameplates panel;
     [809](./809-targeting-and-range.md), answered question 1): the
     original's colours by default, with the fuller set of party,
     friend and dead colours as a switch.

## Open questions

1. **Gamma's curve** (from wow-chat-2026 173b5). The original's Gamma
   slider (-0.5 to 0.5) changes the display's gamma ramp; this client
   would do it in its shaders. Should the slider raise each colour to the
   power 1 / (1 + gamma), or be matched by eye against the original at a
   few positions?

## Related

- docs/048 (the original's menu), [207](./207-camera.md), [310](./310-shadows-and-live-light.md),
  [401](./401-m2-reader.md), [809](./809-targeting-and-range.md).
