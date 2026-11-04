# 703 — Frames: Anchors, Scale, and Layers

*Written 2026-10-16 from the clean-room checklist's section 1
([docs/057](../docs/057-what-the-original-program-decides.md)), where
Phase 7's capstone sketched it as "widget tree + anchor/size primitives
in C, scriptable from Lua".*

## Blocked by

- Nothing for the window-free part, built 2026-11-04 on 702's branch.
  Raising on click, hover and input order wait for the input part (steps
  4 and 5, with 704); the Lua methods for
  [705](./705-interface-functions.md) and
  [701](./701-the-lua-environment.md) (the Lua environment).

## Blocks

- [705](./705-interface-functions.md) (widget methods),
- [704](./704-events-and-the-per-frame-update.md),
  [708](./708-phase-demo.md),
  [710](./710-a-web-browser-addon.md) (its browser frame is a frame
  kind of this tree: `frames_new_frame` with its own type, laid out like
  any other, drawing its page wherever `frames_draw_order` puts it).

## Current behavior

Built 2026-11-04 (branch `claude/frames-and-anchors`, stacked on 702's
`claude/framexml-reader`, not yet merged), as background work the owner
asked for: everything here that needs neither a window, input nor Lua.
Nothing is drawn yet; the only text on screen is still drawn by C.

- `src/071-frames.c` / `.h`: the frame tree behind a `Frames` handle
  (steps 1 to 3, the drawing half of 4, and one quirk switch of step 6).
  Frames and regions (textures, font strings) by id, never destroyed;
  names; parents and children in order, `SetParent` loops refused.
  - **Rectangles** (note 1.1): nine anchor points; each axis found by
    itself from the left, middle and right (bottom, middle, top) the
    anchors give, two of them or one with the set size; size 0 as "no
    size"; anchors over the set size; offsets and own size times the
    effective scale; the layout's waiting list in dependency order, at
    most 32 passes and 6 tries an object, moves under 0.00001 ignored;
    a circle of anchors (and all tied to it) left unplaced and said to
    be one (`FRAMES_IN_LOOP`, `FRAMES_WAITS`); clamping by sliding, with
    clamp insets; a frame tied to one made later placed in the same
    layout. Every read lays out first, so nothing read is stale.
  - **Scale**: the product up the parent chain; 0 or below refused;
    changes under 2.4 × 10⁻⁷ ignored.
  - **Strata and levels** (note 1.2): the eight strata and PARENT; a new
    frame at its parent's strata and level + 1 (MEDIUM, 0 with none),
    following until its own is set; the raise limit (quirk) and its
    correction.
  - **Shown and visible**; **draw layers** with sublevels −8 to 7;
    `frames_draw_order`, bottom to top: strata, level, the newest shown
    on top, then each frame's regions by layer, sublevel and the order
    made.
  - `FramesOptions.resized`: each object whose size changed, after each
    pass, for 704's OnSizeChanged; what it changes is taken in later
    passes.
  - The other quirk switch here: "set size over laid-out size"
    (`FramesOptions.size_from_layout`).
- `src/072-frames-xml.c` / `.h`: 066's objects built in the tree, with
  their children, behind a `FramesXml` handle: `<Size>`, anchors (to XML
  objects or frames the scripts made, by name), `setAllPoints`, named
  parts' default anchors, `hidden`, `scale`, `frameStrata`,
  `frameLevel` with the "XML level 0" quirk and its correction
  (`FramesXmlOptions.level_zero`), `clampedToScreen`, each region's
  `<Layer level>` and `textureSubLevel`; also an object `fxml_create`
  made for CreateFrame with a template. Its warnings go to a callback.
- `scripts/build`: both join CORE, not the reader library (Decisions).

Checked:
- `tests/044-test-client.c`, "frames, anchors and layers", its own block
  before the archives, on C calls and on FrameXML the test writes:
  73 checks. Step 7's first four items and the level item (both
  choices); every anchor point; a left edge and a middle; one anchor and
  no size; a chain ten deep, moved; twenty frames each tied to one made
  after it, each computed once in one layout; a circle of three with a
  fourth tied to it, unplaced and undrawn, then broken; a size handler
  that never settles, stopped at 6 tries and carried to the next
  layout; setAllPoints; scale refused, 0.5 under 2, a parent's change
  reaching its child, a tiny change ignored; strata and levels with
  children following and PARENT; draw order by strata, level and newest
  shown; layers and sublevels with SetDrawLayer; hiding a parent; parent
  loops; clamping with insets; a template's frame from XML with its
  regions, a scaled button and its named parts, `hidden`, both "XML
  level 0" choices, a bad strata, `clampedToScreen`; CreateFrame with a
  template; both choices of "set size over laid-out size"; 400 random
  trees, 12000 changes, laid out as a reference written separately in
  the test lays them out, circles found alike.
- 2026-11-04: all 73 pass in the whole test program (built with
  stand-in raylib headers, as raylib isn't installed here; the earlier
  checks fail there for want of game data, as they always do without
  it) and in the block alone with scripts/build's flags; the same under
  the address and undefined-behaviour checkers (gcc), whose only leak
  reports are the test program's own from stopping at the archives (the
  block alone leaks nothing). Separate runs under the same checkers
  found nothing after one fix (an empty draw order handed to `qsort`):
  16 million random calls of every kind into 071 (with size handlers
  that change frames and make new ones), 20000 random trees against the
  reference, 20000 damaged copies of the test's XML through 066 and 072.

## Intended behavior

The interface's frames are placed, stacked and clicked as the original
places, stacks and clicks them, so the default interface's own XML and
Lua (read from the archives) lay themselves out unchanged, and addons
written for the original see the same rectangles, levels and click
targets. Each rule below is answered in a docs/057 note. The issue
names the note and doesn't repeat it (the owner: long answers live in
the docs; blueprints reference them).

- **A frame's rectangle from its anchors** (note 1.1): nine anchor points,
  each edge found by itself, size zero meaning "no size given", a waiting
  list worked through in dependency order, up to 32 passes and about 6
  tries a frame, changes under 0.00001 ignored, clamping by sliding,
  circular anchors leaving the edge unknown (frame undrawn, unclickable)
  rather than failing.
- **Effective scale** (row "a frame's effective scale"): the product up
  the parent chain, applied to offsets and own size; 0 refused; changes
  under about 2.4 × 10⁻⁷ ignored.
- **Strata and levels** (row "frame strata", note 1.2): the nine strata in
  order; a new frame's starting strata and level; children following a
  change; raising on click to one above the top after closing the gaps;
  the Ctrl/Alt drag mode, off by default.
- **Drawing against input** (note 1.3): drawing bottom to top, newest-shown
  on top within a level, the five draw layers, HIGHLIGHT on hover; input
  from the top down, first enabled frame under the pointer takes it; key
  release and repeat only to the frame that took the press; Print
  Screen's release counted as its press.
- **Locked anchors in combat** refused silently (note 1.1).
- **The frame and region methods** (note 1.15, filled 2026-10-16):
  interface units (the screen is 768 tall at scale 1, and 768 × its
  aspect wide); values read back in the region's own scaled units;
  layout finished before any position or size is read, so never stale;
  on/off arguments read by the note's rules (left out means on, nil
  means off, text by its first letter); the protected methods refused
  only when the region counts as protected, the caller is tainted and
  combat lockdown is on, refused **before** their arguments are checked
  ([709](./709-taint-and-combat-lockdown.md)); and each method's own
  rules: `SetParent` loops refused, `GetRect` and the edges, the width
  and height getters, `SetPoint`, `GetBoundsRect`, strata, levels,
  scale, alpha, IDs, show and hide, draw layers, drag buttons, hit
  insets, backdrops, depth, title regions, regions and children in
  order, animation groups.

### Quirks, each with a correction switch (off by default: the original)

Following docs/057, "The original's quirks". Each switch is a setting the
player can turn on, and both choices are tested.

| Quirk (docs/057) | The original | The correction |
|---|---|---|
| raise limit (1.2) | one call raises a level by at most 128 | raise to the level asked |
| XML level 0 (1.2) | `frameLevel` 0 or less in XML refused with a warning | accept 0 |
| who gets the click (1.3) | among frames of equal strata and level, the one that signed up for input first, which is the one drawn **underneath** | ask the newest-shown first, so the frame on top is clicked |
| on/off argument left out (1.15) | `EnableMouse()` means on, `EnableMouse(nil)` means off | treat left out like nil; expected to be load bearing, since stock code calls these bare to mean "on" |
| set size over laid-out size (1.15) | the width and height getters give the set size before the laid-out one | give the laid-out size whenever there is one; expected to be load bearing |
| `GetBoundsRect` (1.15) | its right and top edges start from the screen's left and bottom, so content off screen still reaches the screen's edge | start from the content alone |
| backdrop edge size (1.15) | a missing `edgeSize` is a share of the screen's diagonal: 32 on 4:3, about 39 on 16:9 | always 32 |
| backdrop colour read back (1.15) | each part as its step × 0.00392156, so full white reads 0.99999780 | divide by 255 |

## Suggested implementation steps

1. ~~The frame tree as a handle (docs/001, "Handles"): frames, their
   parents, nine anchors each, own size, scale, strata, level, shown,
   draw layers.~~ Built (071).
2. ~~Rectangles from anchors with the waiting list and its limits (note
   1.1)~~ (071), worked through twice a frame by 704's update order
   (704 calls `frames_layout`).
3. ~~Effective scale; strata and levels with the starting values and the
   children rule.~~ Built (071).
4. ~~Drawing order~~ (071, `frames_draw_order`) and input order as two
   separate orderings of the same frames; hover and HIGHLIGHT (the draw
   order lists HIGHLIGHT regions with their layer; drawing them only on
   hover is the input part's); key press memory for release and repeat.
5. Raising on click with gap-closing; the Ctrl/Alt drag mode.
6. The correction switches: "raise limit" and "set size over laid-out
   size" are `FramesOptions` fields, "XML level 0" a `FramesXmlOptions`
   field (built, both choices tested); "who gets the click" waits for
   input order; the other four for their methods (705). All to be
   declared in the settings store (`src/014-settings.c`) beside the
   original's own names by the part that first reads the player's
   interface settings (706), as 702's switch is.
7. Tests, each answering a docs/057 note (the first five built; the
   last two wait for input order):
   - a frame anchored left to one frame and right to another gets both
     edges; with only a width and a left anchor, the right edge is
     left + width;
   - a circle of three frames leaves all three unplaced and nothing
     crashes; breaking the circle places them;
   - a frame tied to a frame placed later in the list still places in one
     frame;
   - scale 0 is refused; a child at 0.5 under a parent at 2 is at 1;
   - asking for level 500 from 0 gives 128, and 500 with the correction
     on;
   - two overlapping frames at the same level: the click goes to the
     lower one, and to the upper one with the correction on;
   - a key released over another frame still reaches the frame that
     took its press.

## Decisions

Made 2026-11-04 without the owner (background work); each is easy to
turn round.

- **Notes 1.1, 1.2, 1.3 and 1.15 aren't in this copy of docs/057.**
  Section 1's rows are there with their answers empty and no numbered
  notes follow, so this was built from this issue's own text. Where the
  issue is silent, the choices below; each is one place in 071 or 072.
- **Two files.** 071 is the tree and knows nothing of XML; 072 builds
  066's objects into it. 705 wraps 071 one call a method (`SetPoint` is
  `frames_set_point`, `GetLeft` reads `frames_rect`, `GetWidth` is
  `frames_width`, ...); 704 calls `frames_layout` and runs OnSizeChanged
  from `FramesOptions.resized`.
- **CORE, not the reader library.** Both are window-free, but neither
  reads a file format, and world-edit-to-execute keeps its own editor
  renderer (1109), so nothing there would call a frame layout through
  LuaJIT. The Lua environment goes to CORE too (701).
- **Ids, not pointers.** The original never destroys a frame, so an id
  names one object for the handle's life; it fits a Lua userdata and the
  FFI, and the arrays can grow.
- **Units.** Rectangles are kept in screen units (scale 1) and read back
  divided by the object's effective scale; anchor offsets and the set
  size are multiplied by it.
- **One axis.** Left and right first, then an edge and the middle, then
  one edge or the middle with the set size; of two anchors giving the
  same edge (TOPLEFT and LEFT), the first set gives it. One anchor and no
  size places a 0-wide object at the point, not an unplaced one.
- **The waiting list.** A pass takes the changed objects and all tied to
  them, in dependency order, and computes each whose own anchors, size or
  scale changed or whose anchors' objects moved. Passes come back only
  when `resized` changes things: up to 32, each object computed at most
  6 times a layout; one needing it after that is carried to the next
  layout rather than dropped. A change of an unplaced object's reason
  counts as a move, so what waits on it is told.
- **Circles.** An anchor to the object itself is refused (`W_ERR_REFUSED`,
  as 066 warns of it in XML); a longer circle is accepted and left
  unplaced, its members found as a strongly connected group, the rest
  said to wait on it. Every edge of an object goes unplaced together,
  since each anchor gives both axes.
- **Clamping.** Insets are added to the edges before the test (positive
  left and bottom move inward, positive right and top outward); a frame
  wider than the screen keeps its left edge on, a taller one its top.
- **Starting values.** No parent: MEDIUM, level 0. With one: following
  it (strata PARENT, level parent + 1) until set. A child's own level
  isn't moved by its parent's. Levels are held from 0 to 65535. The raise
  limit counts from the level the frame has when asked; XML's
  `frameLevel` isn't a call, so it has no raise limit.
- **Nine strata** read as UI.xsd's list: PARENT and the eight
  (BACKGROUND to TOOLTIP), as the schema the archives ship is
  remembered; it isn't in this copy to check (Open, 2).
- **Newest shown.** A frame becoming visible (shown, or its parent
  shown, or reparented) gets the newest time, parents before children,
  children in order. Hidden frames are still laid out.
- **Regions** have no scale, strata or level of their own; they draw
  with their frame, by layer, sublevel and the order made. A named part
  of a frame (`NormalTexture` ...) with no anchors fills its frame, and
  `ButtonText` sits at its centre; a named part is in ARTWORK,
  `HighlightTexture` in HIGHLIGHT. A plain `<Texture>` with no anchors
  stays unplaced (the default interface writes `setAllPoints` where it
  wants one filled).
- **Small calls.** A negative size is held at 0; a size, offset or scale
  that isn't a number is refused; `SetAllPoints` is clearing, then
  TOPLEFT and BOTTOMRIGHT; an anchor's "no relativeTo" is the parent at
  the time it is set, and anchors keep what they are tied to through
  `SetParent`; a moved object goes last among its new parent's children.
- **The correction switches are options on the handles**, not yet
  settings, as 702's is (step 6).
- **Sanitizers with gcc.** clang here has no address-checker runtime.

## Open questions

1. **Every Decision above** against notes 1.1, 1.2 and 1.15 when the
   describer fills them, most of all: one anchor and no size; which of
   two anchors giving the same edge wins; whether a circle leaves only
   the edges it touches unknown; when OnSizeChanged runs against the
   passes; the starting level with no parent; a parent's level change
   against a child's own; clamp inset signs; the named parts' defaults.
2. **The ninth strata** (PARENT or WORLD), against UI.xsd on the owner's
   machine.
3. **Raising on click with gap-closing** (step 5) and input order (step
   4, with the click quirk): the input part, with 704.

Standing from before: the interface's thousands of functions, widget
methods and events are to arrive in docs/057 as Appendices B (widget
methods), C (functions) and D (events), grouped by widget type and function family:
names and usage first, behaviour notes family by family afterwards
(decided 2026-10-16). Each widget type's methods become part of this
issue as their appendix fills. Notes as step-shaped as 1.1 stand: the
owner answered docs/057's question 8 on 2026-10-16 ("We can describe
each instruction, but it must be as abstract as possible while still
maintaining accuracy. Lower precision, maintain accuracy.").

## Related

- [docs/006](../docs/006-ui-and-addons.md) (the interface's design),
  [docs/057](../docs/057-what-the-original-program-decides.md) section 1,
  [704](./704-events-and-the-per-frame-update.md),
  [110e](./110e-per-frame-work-as-stations.md) (the interface as a
  station), [710](./710-a-web-browser-addon.md) (a frame kind that lays
  out and draws a page), `src/071-frames.info.md`,
  `src/072-frames-xml.info.md`.
