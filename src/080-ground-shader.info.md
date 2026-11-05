# 080-ground-shader

*Written by `scripts/generate-info` from `080-ground-shader.h`; don't edit, re-run it.*

080-ground-shader.h (issues 173f, 302, 1109) - the ground's shader, as
text, without the window

For a general audience: the ground drawer (038) hands the graphics
card a small program, a shader, that works out each point's colour
from the ground's pictures. It is kept here, apart from the drawer, so
the parts that need no window (the tests, a checking tool) can read
the very text the card is given.

A tile is drawn in four orders (one a quarter: 64 chunks each), not 256
(173u). Every ground picture lives in one texture array (`layers`, one
256 x 256 layer a picture, shared by all tiles); each chunk's four
layer numbers ride on its points (Raylib's tangent slot, `lay`, not
blended across a triangle). The tile's 256 blend pictures sit in one
1024 x 1024 picture (`blend`), 64 x 64 a chunk; `uv2` reaches only the
chunk's own square (from its first point's centre to its last's), so
smoothing never borrows a neighbour's. `uv` repeats each picture 8
times across a chunk (once every 4.2 yards, as the original tiles
them; a cell each), columns east and rows south.

Blending is the original's: each layer laid over the ones below by its
strength, then tinted by the chunk's colours (MCCV; 127 = unchanged, so
0 to 2 times), then darkened where the chunk's baked shadow says (173h:
to 60%, the blend picture's alpha), and under each character by a soft
round patch (a "blob": its centre, its radius; fading out over 3 yards
of height, so a character high above the ground casts none).

A converted map's WC3 ground (078, 1109) is laid cell by cell instead,
as Warcraft III lays it: a tile with WC3 cells has their pieces in a
small picture (`cells`, 256 x 128: 033's ground_prep_cell_texels), read
texel by texel (never smoothed). The point's cell is its chunk's (from
`uv2`: the blend square it is in) and the cell in it (from `uv`); its
place in the cell is what `uv` has past the cell's corner. Each of the
cell's pieces is a quarter of its layer's side a subtile (row sub / 4
from the top, column sub % 4), read a half texel in from its edges and
with the slopes of the cell's own place (so the card picks the same
smaller copy across a cell's edge, where the subtile read jumps); the
lowest whole, each higher one over it by its own alpha. A cell with no
piece (WC3_PIECE_NONE first: a chunk off the WC3 grid) is blended from
its chunk's four pictures as above. Tint, shadow, blobs, light and fog
then as for any ground.

The uniforms and attributes the drawer sets: mvp, eye; blend (Raylib's
first material map), layers (the array's unit), cells (the material's
second map), has_cells (1 when the tile has WC3 cells); sun_dir,
sun_col, shade_col, fog_col, fog_range (037's sky_light_shader); blobs
and nblobs.

Window-free text; compiled only by the drawer, on the main thread.

