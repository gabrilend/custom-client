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

### Video changes wait for Apply, each previewed in the world first

The owner, 2026-10-20: "we should have a list of changes that are
applied when the user hits 'apply' or whatever. One of these might
unload and re-load all the models in the game. We should have a display
next to the option that allows the user to see the effect of their
changes in real-time before they hit apply."

- **Which settings wait.** Only the video panels' settings wait for
  Apply. Everything else, such as mouse speed or how fast the camera
  turns, takes effect the moment it changes, because it's judged by
  using it. A setting outside the video panels may be declared to wait
  when applying it is costly, for example switching the sound output
  device, which restarts the sound system.
- **Nothing needs a restart.** The owner: "we should have none that
  require a restart." Every setting applies inside the running client.
  The store's "needs a restart" mark is replaced by the apply action
  below, and a setting that can't yet apply live isn't finished.
- **The waiting list.** Changing a waiting setting doesn't change the
  store. It adds a line to a list shown in the menu: the setting, its
  value now, its new value, and what applying it will do. Changing the
  same setting again replaces its line; changing it back removes the
  line. **Apply** carries out the whole list, then writes the store and
  `Config.wtf`.
- **Each setting declares its apply action** in the store, beside its
  name, kind and default, chosen from a short table: nothing beyond
  reading the new value; rebuild the textures; unload and reload every
  model; rebuild the shadow maps; recreate the window. Apply runs each
  action once, however many waiting changes need it, in that table's
  order. The list shows the cost before the player commits, for example
  "reloads every model".
- **Closing the menu with changes still waiting** throws them away and
  leaves the store as it was. The owner: "The user might be in combat or
  something!" The thrown-away list is kept in memory until the client
  exits. The next time the menu opens, a box says "You have changes you
  didn't apply. Bring them back, or clear them?" The rest of the menu is
  dimmed and takes no input until one is chosen.

#### Previews are camera views into the world

Beside each waiting setting, a small view shows the effect of the
waiting value. It isn't a picture: it's a second camera looking at a
real place in the world, drawn by the client's own renderer with the
waiting value, so what it shows is what Apply will give. Each preview is
described as data (which place, where its camera starts, what it looks
at) and drawn by one preview viewer, keeping the places apart from the
drawing. Dragging in a preview moves only the viewer's own preview
camera.

- **The dressing room** (model detail, texture size): a dark, endless
  void, something like the Twisting Nether, where dressing-room dummies
  stand, characters of a random race and sex in random clothes, playing
  an animation.
  - **Pause**, a button just below the view, freezes the viewer's own
    picture. The dummies keep moving in the void, so **play** snaps
    back to where they are now.
  - **Redress**, a button beside it, gives the dummy a new random race,
    sex and outfit. The void is a place in the world, so everyone
    looking at it sees the new dummy at once. It can be pressed at most
    once every 5 seconds.
  - **The server holds it.** World changes come only from the server
    (docs/001), so the void, its dummies and each redress are the
    server's. The client asks for a redress, and the server picks the
    new look, keeps the 5-second limit and sends the result to everyone
    watching. That half is server work, not yet planned.
  - **The dummy is drawn by the dressing-up model**, the same interface
    piece the original uses for its dressing room (trying an item on)
    and its character window. The original has one family for all of
    these: a player model, and the dressing-up model, which is a player
    model that can also dress, try on and undress (docs/057
    Appendix B). This client builds that piece once
    ([703](./703-frames-anchors-and-layers.md),
    [705](./705-interface-functions.md)) over the character drawing
    ([404](./404-characters-and-equipment.md)), and the dressing room,
    the character window and this preview all use it.
- **Shadows:** a real place, such as a house somewhere in Arathi
  Highlands, with the camera on the house's shadow. Moving a shadow
  setting redraws it at once.
- **Other settings** get previews the same way as their features are
  built. A setting with no preview yet says so beside it.
- **What a preview costs.** The second camera's place must be loaded
  alongside the place the player is in: its ground, buildings and
  models, at the waiting settings. The preview's own copies (a model at
  a detail limit the world isn't using) are freed when the menu closes.
- **At the login screen there are no previews.** The menu opened there
  is a simpler one: the same panels and the same waiting list and
  Apply, without the preview views beside the settings (answered
  question 4). The previews appear once a world is loaded.

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
4. The waiting list and Apply: which settings wait (the video panels,
   plus any declared to), each setting's apply action declared in the
   store in place of "needs a restart", the list drawn in the menu,
   Apply running each action once, and the "bring them back, or clear
   them?" box over the dimmed menu. Tests:
   - mouse speed changes at once and adds no line;
   - changing view distance twice and then back leaves the list empty;
   - two waiting model-detail changes reload the models once;
   - closing the menu leaves the store unchanged; opening it again
     shows the box, and "bring them back" restores the same list;
   - no declared setting has an apply action of "restart".
5. The preview viewer, which draws a second camera's view of a place
   (data) with waiting values in a small view; the pause and play that
   freeze and snap the viewer's picture; dragging that moves only the
   preview camera. Then the first places: the dressing room in the void
   with its redress button (needs the dressing-up model from
   [705](./705-interface-functions.md), the character drawing in
   [404](./404-characters-and-equipment.md),
   [401](./401-m2-reader.md) and
   [402](./402-skeletons-and-animation.md), and the server's half), and
   the house in Arathi Highlands for shadows (needs
   [310](./310-shadows-and-live-light.md)). Tests:
   - the model-detail preview at 21 bones draws a different detail file
     from the one at 256, while the world keeps its own;
   - after pause, five seconds, then play, the dummy's animation is
     where the void's is, not five seconds behind;
   - a second redress within 5 seconds is refused by the server, and
     the client says so.
6. The project's own settings, which the original doesn't have, each
   marked as the project's in its panel so a player can tell them from
   the original's:
   - **Model detail** (Effects panel; [401](./401-m2-reader.md),
     answered question 1): the bone limit a model's detail file is
     chosen by, one of 256, 64, 53 or 21, default 64 as on a typical
     player's machine. Its apply action reloads every model; its
     preview is the dressing room (step 5).
   - **Selection colours** (Names and nameplates panel;
     [809](./809-targeting-and-range.md), answered question 1): the
     original's plainer colour variant by default (until docs/057 says
     which variant the original uses where), with its fuller variant,
     the one with party, friend and dead colours, as a switch.

## Open questions

None.

## Answered

1. ~~**Gamma's curve**~~ (from wow-chat-2026 173b5). Matched by eye
   against the original, by a tool (2026-10-20). The original's Gamma
   slider (-0.5 to 0.5) changes the display's gamma ramp; this client
   does it in its shaders. Offered a formula (each colour raised to the
   power 1 / (1 + gamma)) or matching by eye, the owner: "matched by
   eye. We can automate that. Just, adjust the setting, then take a
   screenshot, then adjust it again, take another screenshot, etc." So
   a tool steps the slider through a set of positions; at each, it sets
   the value in the original client's settings file, starts the
   original at a fixed place and takes a screenshot, then does the same
   with this client (`scripts/run --shot`). It measures the brightness
   of the same patches in each pair, and fits this client's curve to
   the original's. Every screenshot is kept and filed in the
   development history, as the house rule says. Test: at each measured
   position, the fitted curve puts this client's patches within a
   small set difference of the original's.

2. ~~**Closing the menu with changes still waiting.**~~ Thrown away, and
   offered back the next time the menu opens (2026-10-20). The owner:
   "thrown away. The user might be in combat or something! But the next
   time we open the settings menu before the client restarts, we should
   say 'You have unsaved changes because you didn't click "apply" -
   reload them, or clear?' and we can dim out the rest of the menu except
   that box so they know where to focus their attention."
3. ~~**Which panels wait for Apply.**~~ Only video (2026-10-20). Offered
   mouse speed and camera turning as things best judged by using them,
   the owner: "those are good examples. Yes those should apply
   instantly. Only video settings I think should be queue-and-apply.
   Maybe specific exceptions in the UI or whatever, or audio..."
4. ~~**Previews with no world to look into.**~~ A simpler menu at the
   login screen (2026-10-20). The menu can be opened there before any
   world is loaded or server connected, and the dressing room's dummies
   are the server's. The owner: "let's have a simpler settings menu from
   the login page, without the fancy galleries." So the login screen's
   menu has the same panels, waiting list and Apply, and no previews.

## Related

- docs/048 (the original's menu), [207](./207-camera.md), [310](./310-shadows-and-live-light.md),
  [401](./401-m2-reader.md), [402](./402-skeletons-and-animation.md),
  [404](./404-characters-and-equipment.md), [703](./703-frames-anchors-and-layers.md),
  [705](./705-interface-functions.md), [809](./809-targeting-and-range.md).
