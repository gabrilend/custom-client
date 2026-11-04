# 071-frames

*Written by `scripts/generate-info` from `071-frames.h`; don't edit, re-run it.*

071-frames.h (issue 703) - the interface's frames: the tree, anchors into rectangles, scale, strata, levels, layers

For a general audience: every window, button and bar of the interface
is a frame, and every picture or line of text on one is a region (a
texture or a font string). A frame isn't given a position on screen.
It is tied by its corners and edges ("anchors") to other frames: "my
top left at the bottom left of that one, 4 lower". This works out,
from those ties, each frame's rectangle on screen, the way the
original works it out, so the default interface's own files and every
addon find their frames where they expect them. It also keeps which
frames lie above which (strata and levels), the order a frame's
pictures are drawn in (layers), and which frames are shown.

Nothing here draws, reads input or runs Lua. Drawing walks
frames_draw_order; input, hover and raising on click are the input
part's (703 steps 4 and 5); the Lua methods (705) wrap these calls one
for one (SetPoint is frames_set_point, GetLeft reads frames_rect, ...);
the per-frame update (704) calls frames_layout twice a frame and runs
OnSizeChanged from FramesOptions.resized. 072-frames-xml makes frames
from the interface's XML (066).

The rules are docs/057's notes 1.1, 1.2 and 1.15 as issue 703 gives
them (the notes themselves are empty in this copy of docs/057; where
the issue is silent, 703's Decisions say what was chosen):
  - Units: interface units. The screen is 768 tall and 768 x its aspect
    wide, at scale 1, with y going up (the bottom edge is 0). Inside,
    every rectangle is kept in screen units; each call takes and gives
    values in the object's own units, which are screen units divided
    by its effective scale.
  - Effective scale: the product of the scales up the parent chain. A
    region has no scale of its own and takes its frame's. A scale of 0
    (or below, or not a number) is refused; a change smaller than
    FRAMES_SCALE_EPSILON is ignored.
  - Anchors: nine points; one anchor a point, a second to the same
    point replacing the first. Each anchor ties a point of the object
    to a point of another object (or the screen), plus an offset in the
    object's own units. Each axis is found by itself: across, the
    anchors give the left edge, the middle or the right edge (TOPLEFT,
    LEFT and BOTTOMLEFT give the left; TOP, CENTER and BOTTOM the middle;
    the rest the right); two of them place the axis (left and right
    first), one of them with the set width places it too; the same up
    and down. A size of 0 means no size was given: one edge and no size
    makes the object 0 wide there. Anchors win over the set size.
  - Order: layout is a waiting list worked through in dependency order
    (an object after everything it is anchored to), in passes of at
    most FRAMES_MAX_PASSES, each object computed at most
    FRAMES_MAX_TRIES times in one layout (one still needing it then
    waits for the next layout); a change smaller than
    FRAMES_MOVE_EPSILON doesn't count as a change, so it moves nothing
    tied to it. An object anchored round in a circle, and everything
    tied to one, is left unplaced: no edges, not drawn, never an error.
    An object tied to one made after it is placed in the same layout.
  - Clamping to the screen slides an object back inside the screen
    (by its clamp insets), and what is tied to it follows.
  - Strata: the eight in order (FramesStrata) and PARENT, "the parent's".
    A new frame takes its parent's strata and the parent's level + 1
    (MEDIUM and 0 with no parent), and keeps following the parent until
    its own is set. Setting a level raises it by at most
    FRAMES_MAX_RAISE in one call (a quirk; FramesOptions.raise_unlimited
    corrects it).
  - Shown and visible: an object is visible when it and every frame up
    its parent chain are shown. The newest-shown frame is on top of the
    others of its strata and level.
  - Draw layers: a region is in one of five layers (FramesLayer), with a
    sublevel from FRAMES_SUBLEVEL_MIN to FRAMES_SUBLEVEL_MAX; a frame's
    regions draw by layer, then sublevel, then the order they were made.

Objects are never destroyed (the original has no way to): an id names
the same object for the handle's life. Ids are 0 and up; FRAMES_SCREEN
names the screen where an anchor or parent may be the screen, and
FRAMES_NONE no object.

## What it offers

- `type FramesPoint`
  The nine points of a rectangle an anchor ties, in the original's order.
- `type FramesStrata`
  Strata, bottom to top; PARENT is "the parent's" (MEDIUM with none).
- `type FramesLayer`
  A region's draw layer, bottom to top. HIGHLIGHT is drawn only while the
  pointer is over its frame (the input part decides; the draw order lists
  it with its layer).
- `type FramesUnplaced`
  Why an object has no rectangle (frames_unplaced).
- `type FramesAnchor`
  One anchor as it was set: the object's point, what it is tied to (an
  id or FRAMES_SCREEN) and that one's point, the offset in the object's
  own units.
- `type FramesRect`
  A laid-out rectangle: `placed` 0 when it has none (and the edges are 0).
- `type FramesDrawItem`
  One step of drawing (frames_draw_order): a frame itself (its
  backdrop), or one of its regions, bottom to top.
- `type FramesOptions`
  How a handle behaves, field by field (all zero is fine: the original):
    raise_unlimited   the correction for the quirk "raise limit" (703): 0,
        a level-setting call raises by at most FRAMES_MAX_RAISE; 1, to
        the level asked
    size_from_layout  the correction for "set size over laid-out size"
        (703): 0, frames_width and frames_height give the set size when
        there is one, else the laid-out one; 1, the laid-out one whenever
        the object is placed
    ctx      handed back to `resized`
    resized  after each layout pass, each object whose laid-out width or
        height changed in it, in the order computed (OnSizeChanged, 704).
        It may change anchors and sizes; the next pass takes them. NULL:
        nobody is told
- `typedef struct Frames Frames;`
  The frame tree (the handle: docs/001, "Handles"): every frame and
  region, the screen's size (1024 x 768 pixels until set). `opt` is
  copied; NULL is all zero. NULL when there is no memory, with `why`.
- `Frames *frames_new(const FramesOptions *opt, WWhy *why);`
- `void frames_free(Frames *f);`
- `WErr frames_set_screen(Frames *f, double width_px, double height_px, WWhy *why);`
  The screen's size in pixels (only its aspect counts): the screen is
  FRAMES_SCREEN_HEIGHT tall and that x width / height wide. W_ERR_REFUSED
  for a size not above 0.
- `double frames_screen_width(const Frames *f);`
- `int frames_new_frame(Frames *f, const char *type, const char *name, int parent, WWhy *why);`
  A new frame of `type` (Frame, Button ...: kept as given) named `name`
  (NULL: unnamed; "$parent" is 066's to work out, not this), under
  `parent` (a frame's id, or FRAMES_NONE): shown, no anchors, size 0,
  scale 1, its parent's strata and level + 1. Its id, or FRAMES_NONE with
  `why`: W_ERR_REFUSED when the parent isn't a frame, W_ERR_FULL.
- `int frames_new_region(Frames *f, const char *type, const char *name, int parent, FramesLayer layer, int sublevel, WWhy *why);`
  A new region of `type` (Texture, FontString ...) on the frame `parent`,
  in `layer` at `sublevel`: shown, no anchors, size 0. Its id, or
  FRAMES_NONE with `why`: W_ERR_REFUSED when the parent isn't a frame or
  the layer or sublevel is out of range, W_ERR_FULL.
- `int frames_find(const Frames *f, const char *name);`
  The object made last with this name, or FRAMES_NONE.
- `int frames_count(const Frames *f);`
- `int frames_is_region(const Frames *f, int id);`
- `const char *frames_type(const Frames *f, int id);`
- `WErr frames_set_parent(Frames *f, int id, int parent, WWhy *why);`
  SetParent: `parent` a frame's id or FRAMES_NONE (a region needs a
  frame). The object goes last among the new parent's children; anchors
  keep what they are tied to. W_ERR_REFUSED: a parent that is the object,
  or under it (a loop), or isn't a frame. W_ERR_NOT_FOUND: no such id.
- `int frames_child_count(const Frames *f, int id);`
  A frame's child frames, and its regions, each in the order they came.
- `int frames_child_at(const Frames *f, int id, int i);`
- `int frames_region_count(const Frames *f, int id);`
- `int frames_region_at(const Frames *f, int id, int i);`
- `WErr frames_set_point(Frames *f, int id, FramesPoint point, int relative, FramesPoint relative_point, double x, double y, WWhy *why);`
  Ties `point` of the object to `relative_point` of `relative` (an id,
  FRAMES_SCREEN, or FRAMES_NONE for its parent: the screen with none),
  offset x, y in its own units; replaces an anchor at the same point.
  W_ERR_REFUSED: tied to itself, a point out of range, an offset not a
  number. W_ERR_NOT_FOUND: no such id (either). A circle of anchors is
  accepted, and left unplaced by layout.
- `void frames_clear_points(Frames *f, int id);`
- `WErr frames_set_all_points(Frames *f, int id, int relative, WWhy *why);`
  Clears the anchors, then ties TOPLEFT and BOTTOMRIGHT to the same
  points of `relative` (FRAMES_NONE: the parent, else the screen).
- `int frames_point_count(const Frames *f, int id);`
- `int frames_point_at(const Frames *f, int id, int i, FramesAnchor *out);`
  The i-th anchor, in the order set (a replaced one keeps its place): 1, or 0 past the end.
- `WErr frames_set_width(Frames *f, int id, double w, WWhy *why);`
  The set size in the object's own units. Below 0 is held at 0; W_ERR_REFUSED when not a number.
- `WErr frames_set_height(Frames *f, int id, double h, WWhy *why);`
- `void frames_set_clamped(Frames *f, int id, int on);`
  Sliding back inside the screen (SetClampedToScreen), and the clamp
  insets (SetClampRectInsets): own units added to the left, right, top
  and bottom edges before the test. A positive left inset moves the
  tested left edge right, so that much of the object may go off the
  screen's left; a negative right inset does the same on the right.
- `void frames_set_clamp_insets(Frames *f, int id, double left, double right, double top, double bottom);`
- `void frames_layout(Frames *f);`
  Works out every rectangle that needs it (704 calls it twice a frame).
  Changes made by `resized` are taken in later passes of the same call.
- `void frames_rect(Frames *f, int id, FramesRect *out);`
  The rectangle in the object's own units / in screen units.
- `void frames_rect_screen(Frames *f, int id, FramesRect *out);`
- `FramesUnplaced frames_unplaced(Frames *f, int id);`
- `double frames_width(Frames *f, int id);`
  GetWidth and GetHeight, by FramesOptions.size_from_layout: own units.
- `double frames_height(Frames *f, int id);`
- `double frames_set_width_of(const Frames *f, int id);`
  The size as set, own units.
- `double frames_set_height_of(const Frames *f, int id);`
- `double frames_scale(const Frames *f, int id);`
  {{{ scale (SetScale, GetScale, GetEffectiveScale)
- `double frames_effective_scale(Frames *f, int id);`
- `WErr frames_set_strata(Frames *f, int id, FramesStrata strata, WWhy *why);`
  FRAMES_STRATA_PARENT follows the parent again. Children following this
  frame move with it. W_ERR_REFUSED: a region, a strata out of range.
- `WErr frames_set_level(Frames *f, int id, int level, WWhy *why);`
  The level asked, held from 0 to FRAMES_MAX_LEVEL and, by the quirk,
  raised at most FRAMES_MAX_RAISE above where it was; children following
  it go to one above. W_ERR_REFUSED: a region.
- `int frames_level(Frames *f, int id);`
- `WErr frames_set_level_direct(Frames *f, int id, int level, WWhy *why);`
  The same, set as made from XML (072): no raise limit.
- `void frames_show(Frames *f, int id);`
  {{{ shown, hidden, visible (Show, Hide, IsShown, IsVisible)
- `void frames_hide(Frames *f, int id);`
- `int frames_is_shown(const Frames *f, int id);`
- `int frames_is_visible(Frames *f, int id);`
- `WErr frames_set_draw_layer(Frames *f, int id, FramesLayer layer, int sublevel, WWhy *why);`
  {{{ draw layers (SetDrawLayer, GetDrawLayer): regions only
- `void frames_draw_layer(const Frames *f, int id, FramesLayer *layer, int *sublevel);`
- `int frames_draw_order(Frames *f, FramesDrawItem *out, int cap);`
  {{{ the order of drawing: every visible, placed frame bottom to top
  (strata, level, then the one shown longest ago first), each followed
  by its visible, placed regions (layer, sublevel, then the order made).
  Lays out first. Writes at most `cap` items into `out`; returns how many
  there are in all.
- `FramesPoint frames_point_from_name(const char *name);`
  {{{ names for 705's string arguments, any case: FRAMES_POINT_COUNT,
  FRAMES_STRATA_COUNT, FRAMES_LAYER_COUNT when not one; and back
- `FramesStrata frames_strata_from_name(const char *name);`
- `FramesLayer frames_layer_from_name(const char *name);`
- `const char *frames_point_name(FramesPoint p);`
- `const char *frames_strata_name(FramesStrata s);`
- `const char *frames_layer_name(FramesLayer l);`
