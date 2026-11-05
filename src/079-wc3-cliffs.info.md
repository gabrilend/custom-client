# 079-wc3-cliffs

*Written by `scripts/generate-info` from `079-wc3-cliffs.h`; don't edit, re-run it.*

079-wc3-cliffs.h (issues 302, 1109) - a converted map's cliffs as
Warcraft III builds them: which cliff model stands on which cell

For a general audience: Warcraft III's ground has levels ("cliff
levels": 2 is the usual ground, each level 128 units higher). Where a
cell's four corners stand on different levels, the game doesn't draw
the ground there: it stands a cliff model on the cell, chosen by how
high each corner is over the lowest. The converter (world-edit-to-
execute) gives the converted ground those heights as steep slopes;
with the player's own Warcraft III install (the assetWc3 setting) this
works out which of WC3's own cliff models stands on each such cell, so
the doodad drawer (042) draws them as it draws any WC3 model, and the
ground painter (078) lays the cliff's own ground under it.

A cell is a cliff cell when its corners are on different levels and
none is on a ramp (063's rule for a bare cell), and a ramp cell when
they are on different levels and one is (issue 1109's step 1f). Its
model, as the published map viewers build it:
  Doodads\Terrain\<dir>\<dir><letters><variation>.mdx
  dir        its cliff tileset's cliffModelDir in TerrainArt\
             CliffTypes.slk ("Cliffs", "CityCliffs"); "Cliffs" when
             the sheet has no row or no such cell
  letters    each corner's level over the cell's lowest, as a letter
             ('A' level, 'B' one up, 'C' two up), north-west,
             north-east, south-east, south-west ("AABA": the
             south-east corner one level up); a corner more than two
             levels up is cut to 'C' (no model reaches further)
  variation  its south-west point's cliff variation (0-7), or the
             highest below it that some place has, else 0
A ramp cell's is the same in its tileset's rampModelDir ("CliffTrans";
"CliffTrans" when the sheet has none), its letters a ramp corner's
'L' on the lowest level and 'H' one up ('D' two: 'L' less four a
level), a corner off the ramp's 'A', 'B' or 'C' as a cliff's
("HLLH": a ramp down to the east). A ramp cell on one level (the
ramp's lower half) gets no model: the ramp model of the cell beside
it reaches over it (1109's decision 51).
The cliff tileset is its south-west point's, else the first of its
south-east, north-west and north-east points' that the map lists; a
cell none of whose corners names one gets no model (and is laid as
before). The model's cliff picture (replaceable picture 11) is drawn
with its tileset's texDir\texFile.blp, or, with no row for it in the
sheet, TerrainArt\<id>.blp, which no place has: the checker. The
model stands on its cell's south-west corner moved one cell east
(WC3's cliff models lie west of their origin, as the viewers place
them), at (base level - 2) x 128 units plus its corners' mean height,
turned as WC3 lays models, unbent (the game bends each point to the
ground under it; issue 1109's decision 46).

The cliffs list holds both: a ramp's has `ramp` set, and the counts
keep them apart.

Window-free: in the test program and the executable (it asks the
resolver which variations exist). Opening reads everything once; after
it the handle is only read, so it is safe on any thread.

## What it offers

- `type Wc3CliffType`
  One of the map's cliff tilesets. Field by field:
    id        its four letters ("CLdi"), ended by a zero
    model_dir its models' folder under Doodads\Terrain (cliffModelDir)
    picture   its cliff picture (texDir\texFile.blp), or TerrainArt\<id>.blp
              when the sheet has no row for it
    ground    the ground tileset laid under its cliffs (groundTile), "" none
    ramp_dir  its ramps' folder under Doodads\Terrain (rampModelDir)
    known     1 when the sheet has a row for it
- `type Wc3Cliff`
  One cliff model on a cell. Field by field:
    i, j    its cell (column from the west, row from the south)
    type    its cliff tileset (into wc3_cliffs_type)
    base    its lowest corner's cliff level
    model   its model's name (into wc3_cliffs_model)
    ramp    1: a ramp's model (its cell keeps its ground: 078)
    x, y, z its origin in WC3's world (units): the cell's south-west
            corner one cell east, at the height above
- `type Wc3CliffCounts`
  What opening found.
- `typedef struct Wc3Cliffs Wc3Cliffs;`
- `WErr wc3_cliffs_open(Resolver *r, MpqChain *chain, const W3eGround *g, Wc3Cliffs **out, WWhy *why);`
  Reads TerrainArt\CliffTypes.slk (077) through `r` and works out every
  cliff of `g` (which must outlive the handle), asking the resolver which
  models exist. W_OK with `*out` (freed by wc3_cliffs_free);
  W_ERR_NOT_FOUND, with why, with no resolver or no sheet in any place;
  W_ERR_CORRUPT from 077 with the sheet's name; a resolver's other code
  from asking for a model, with its name.
- `void wc3_cliffs_free(Wc3Cliffs *c);`
- `int wc3_cliffs_types(const Wc3Cliffs *c);`
- `const Wc3CliffType *wc3_cliffs_type(const Wc3Cliffs *c, int k);`
- `int wc3_cliffs_count(const Wc3Cliffs *c);`
- `const Wc3Cliff *wc3_cliffs_get(const Wc3Cliffs *c, int k);`
- `void wc3_cliffs_counts(const Wc3Cliffs *c, Wc3CliffCounts *out);`
- `int wc3_cliffs_at(const Wc3Cliffs *c, int i, int j);`
  The cliff on cell (i, j) (into wc3_cliffs_get), or -1: none, or off
  the grid.
- `const char *wc3_cliffs_model(const Wc3Cliffs *c, int m);`
  Model m's name as the drawer takes it: its path, then 076's
  PLACED_REPLACE_MARK, "11=" and its cliff picture; NULL past the end.
- `int wc3_cliff_letters(int nw, int ne, int se, int sw, char out[5], int *steep);`
  A cell's letters (see above) from its corners' cliff levels, ended by
  a zero; returns the lowest level. `steep` (may be NULL) is set to 1
  when a corner was cut to 'C'.
- `int wc3_ramp_letters(int nw, int ne, int se, int sw, int rnw, int rne, int rse, int rsw, char out[5], int *steep);`
  A ramp cell's letters (see above) from its corners' levels and ramp
  marks (1: on the ramp); returns the lowest level. `steep` as above.
