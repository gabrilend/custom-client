# 703 — Frames: Anchors, Scale, and Layers

*Written 2026-10-16 from the clean-room checklist's section 1
([docs/057](../docs/057-what-the-original-program-decides.md)), where
Phase 7's capstone sketched it as "widget tree + anchor/size primitives
in C, scriptable from Lua".*

## Blocked by

- [702](./702-the-xml-reader.md); and the Lua environment, Phase 7's
  first piece, sketched in its capstone and not yet written as an issue.

## Blocks

- [705](./705-interface-functions.md) (widget methods),
- [704](./704-events-and-the-per-frame-update.md),
  [708](./708-phase-demo.md).

## Current behavior

Nothing of the interface exists. There's no frame tree, no anchors and no
strata; the only text on screen is drawn by C
([docs/006](../docs/006-ui-and-addons.md), "Phase plan").

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

1. The frame tree as a handle (docs/001, "Handles"): frames, their
   parents, nine anchors each, own size, scale, strata, level, shown,
   draw layers.
2. Rectangles from anchors with the waiting list and its limits (note
   1.1), worked through twice a frame by 704's update order.
3. Effective scale; strata and levels with the starting values and the
   children rule.
4. Drawing order and input order as two separate orderings of the same
   frames; hover and HIGHLIGHT; key press memory for release and repeat.
5. Raising on click with gap-closing; the Ctrl/Alt drag mode.
6. The three correction switches, declared in the settings store
   (`src/014-settings.c`) beside the original's own names.
7. Tests, each answering a docs/057 note:
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

## Open questions

None yet. The interface's thousands of functions, widget methods and
events are to arrive in docs/057 as Appendices B (widget methods), C
(functions) and D (events), grouped by widget type and function family:
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
  station).
