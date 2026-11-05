# 081-wc3-water

*Written by `scripts/generate-info` from `081-wc3-water.h`; don't edit, re-run it.*

081-wc3-water.h (issues 302, 1109) - a converted map's water as
Warcraft III draws it, without the window

For a general audience: world-edit-to-execute's converter turns a WC3
map's water into the game's own water (ground files' liquid, kind 1:
plain water), drawn with the game's lake pictures and the zone's
colours (040). A player who has Warcraft III sees the map's own water
instead: WC3's rippling pictures, coloured by how deep the water is at
each tile point, from pale shallows to dark depths, and tinted as the
map says. This reads what that needs; the water drawer (040, through
082's meshes and shader) draws with it (issue 1109's water plan).

What WC3 reads (the published map viewers' reading, HiveWE's): the
sheet TerrainArt\Water.slk, its row named by the map's main tileset's
letter and "Sha" ("LSha" for Lordaeron summer), with these columns:
  texFile   the pictures' path less the frame: frame k is texFile, k
            in two digits, ".blp" (ReplaceableTextures\Water\Water00.blp)
  numTex    how many frames, shown one after another (the ripples)
  texRate   frames a second
  height    the drawn surface's offset from the stored water level, in
            cells (128 WC3 units); read and reported, not used: the
            surface stays 063's (decision 66)
  Smin_A, Smin_R, Smin_G, Smin_B, Smax_..., Dmin_..., Dmax_...: the
            colours (0 to 255, alpha first) shallow water goes between,
            and deep water
The colour at a point by its depth d (water z less ground z, 063's, in
cells): up to 64/128 shallow, from Smin at 10/128 (and less) to Smax at
64/128; past that deep, from Dmin at 64/128 to Dmax at 72/128 (and
more). Then, when the map's war3map.w3i (083) is found and its tint's
flag is set, each colour is tinted by it: each of red, green, blue and
alpha times the tint's over 255 (decision 72).

Where the files come from: the sheet and the frames through the
resolver (060): the player's packs, the server's, overlays (the map's
own imports), the player's Warcraft III archives; never the checker.
war3map.w3i by its name, from the converted build folder's root
(world-edit-to-execute's convert copies it there since its draft PR
#247; without it the water is untinted). With no sheet, no row for the
tileset, or a frame no place has, there is no WC3 water and the
converter's stays.

Window-free: the test program and the executable. Opening reads and
decodes everything once; after it the handle is only read.

## What it offers

- `type Wc3WaterKind`
  The map's water, from its tileset's row. Field by field:
    row          the sheet's row ("LSha"), ended by a zero
    tex_file     texFile, as the sheet gives it
    frames       numTex, 1 to WC3_WATER_FRAMES_MAX
    rate         texRate, frames a second (above 0; 0 in the sheet: 1)
    height       the sheet's height, in cells (0 when it gives none)
    shallow_min, shallow_max, deep_min, deep_max   RGBA, 0 to 255
    tinted       1 when the map's war3map.w3i tints its water
    tint         that tint, RGBA (255s when untinted)
- `type Wc3WaterFrame`
  One frame: its path (tex_file, two digits, ".blp"), where it came
  from, and its pixels (width x height RGBA, rows from the top).
- `type Wc3WaterCounts`
  What the map's points come to.
- `typedef struct Wc3Water Wc3Water;`
- `WErr wc3_water_open(Resolver *r, MpqChain *chain, const W3eGround *g, double scale, double x0, double y0, Wc3Water **out, WWhy *why);`
  Reads Water.slk and every frame of the map's tileset's row through `r`
  (a test-map run's resolver), for the map's ground `g` (063: its main
  tileset's letter, its points; it must outlive the handle), and the
  map's war3map.w3i when some place has it. `scale`, `x0`, `y0`:
  map.lua's rule (wow_x = x0 + scale * wc3_y; wow_y = y0 - scale *
  wc3_x), for wc3_water_at. W_OK with
  `*out` (freed by wc3_water_free); W_ERR_NOT_FOUND, with why, when there
  is no resolver, no Water.slk in any place (no Warcraft III install),
  no row for the tileset, or a frame no place has (naming it): the
  converter's water stays; W_ERR_CORRUPT for a sheet 077 refuses, a row
  whose texFile is empty or numTex outside 1 to WC3_WATER_FRAMES_MAX, a
  colour outside 0 to 255, or a frame 017 can't decode (naming it);
  war3map.w3i's own refusals (083: W_ERR_CORRUPT, W_ERR_UNSUPPORTED),
  naming it; W_ERR_REFUSED for a scale not above 0.
- `void wc3_water_free(Wc3Water *w);`
- `const Wc3WaterKind *wc3_water_kind(const Wc3Water *w);`
- `Wc3WaterCounts wc3_water_counts(const Wc3Water *w);`
- `const W3iInfo *wc3_water_info(const Wc3Water *w);`
  The map's settings as 083 read them, or NULL when no place has its
  war3map.w3i.
- `int wc3_water_frame_at(const Wc3Water *w, double seconds);`
  The frame shown `seconds` after the start: floor(seconds x rate),
  round the frames (0 for a time below 0).
- `void wc3_water_colour(const Wc3WaterKind *k, double depth, uint8_t out[4]);`
  WC3's colour for water `depth` cells deep (the rule above, tinted when
  `k` is), RGBA.
- `int wc3_water_point(const Wc3Water *w, int i, int j, double *depth, uint8_t rgba[4]);`
  Tile point (i, j) (rows from the south): 1 when wet, with its depth in
  cells (063's water z less ground z, over 128) and its colour when
  asked; 0 when dry or outside the grid.
- `int wc3_water_at(const Wc3Water *w, double x, double y, double *z, uint8_t rgba[4]);`
  World place (x, y) (yards, by the map's rule) to its nearest tile
  point: 1 when the place is that point (within a hundredth of a yard),
  0 when it lies elsewhere (between points, or off the grid: then the
  grid's nearest point). Gives that point's water surface in the world
  (`z`, yards: 063's water z times the scale) and its colour by its
  depth (a point dry or under the ground: as shallow as the rule goes),
  each when asked.
