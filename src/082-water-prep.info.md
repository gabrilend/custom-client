# 082-water-prep

*Written by `scripts/generate-info` from `082-water-prep.h`; don't edit, re-run it.*

082-water-prep.h (issues 173r, 302, 1109) - the water's meshes and
shader, without the window

For a general audience: the water drawer (040) turns each ground
file's water layer into a mesh, two triangles a wet cell, and hands
the graphics card a small program, a shader, that colours it. Both are
worked out here, apart from the drawer, so the parts that need no
window (the tests, a checking tool) can read the very points and text
the card is given.

A layer's points (water_prep_layer): each wet cell's four corners
(two triangles), at the heights the file gives, relative to the drawing
origin; the picture repeats once a cell (4.2 yards), placed by world
position so neighbouring layers meet; each point carries the file's
depth there (over 255). A converted map's water, when a player has
Warcraft III (081, issue 1109's water plan), is drawn as WC3 draws it:
each point on a WC3 tile point takes that point's own water surface
and its colour by its depth (tinted as the map says), so a dry corner
of a wet cell sinks to WC3's surface under the bank rather than the
converter's level over it (decision 73); a point off WC3's grid (water
the converter adds past the map's edge) keeps the file's height and
takes the colour of the grid's nearest point.

The shader (WATER_VS, WATER_FS): the ripple picture, tinted by the
zone's water colours (water and sea), lit a little by the sun, fogged,
see-through. The tint goes from the near colour to the deep one, and
the opacity from the shallow to the deep (the light's water_alpha), by
the point's depth share. The straight 0-255 reading is this client's
guess at the original's curve (check list). Lava and slime (`raw`):
the picture alone, solid. WC3's water (`wc3`): the picture times the
point's colour, alpha too, unlit (as WC3 draws it), fogged as the rest
of the world is.

Window-free: the test program and the executable.

## What it offers

- `type WaterVerts`
  A layer's points, the caller's (water_verts_free). Field by field:
    count        points: 6 a wet cell (two triangles)
    vertices     x, y, z each, relative to the drawing origin
    texcoords    the picture's place (2 each)
    texcoords2   x: the file's depth over 255; y: 0
    colors       RGBA each, WC3's colour; NULL for the converter's water
    on_grid      WC3's water: points on a WC3 tile point
    off_grid     WC3's water: points off it (the file's height kept)
    moved        WC3's water: points whose height WC3's surface changed
                 (by more than a thousandth of a yard)
- `WErr water_prep_layer(const AdtTile *t, int layer, double ox, double oy, const Wc3Water *wc3, WaterVerts *out, WWhy *why);`
  Tile `t`'s water layer `layer` as points, placed relative to (`ox`,
  `oy`), drawn as WC3's when `wc3` is given. W_OK with `*out` (count 0
  for a layer with no wet cell); W_ERR_REFUSED for a layer past the
  tile's.
- `void water_verts_free(WaterVerts *v);`
