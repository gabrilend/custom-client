# 103-area

*Written by `scripts/generate-info` from `103-area.h`; don't edit, re-run it.*

103-area.h (issue 1109 step 1aa) - the zone she stands in, and its name

For a general audience: the game names the part of the world you are
in (a zone, an "area"). Each 33-yard chunk of the ground files says
which row of AreaTable.dbc it belongs to, and that row holds the
zone's name. World-edit-to-execute's converter writes both for a
converted map: one AreaTable row named after the map (its
src/wowbuild/registration.lua and dbc.lua) and that row's number in
every chunk of every ground file (src/wowbuild/adt.lua, MCNK offset
52). The client read neither. This finds the chunk under a point and
the row it names, with no window, so the window (043) can put the
zone's name in its title and a run with no window (089) can say and
count where she stood.

The chunk under a point is the one whose north-west corner (MCNK
offset 104, which 020 already reads) lies within a chunk's side north
and west of it, 0.01 yards given for the file's rounding, the first in
file order on a shared edge. The columns are found by name in the
layouts (097: AreaTable's ID, ContinentID, ParentAreaID and AreaName_enUS,
the first of AreaName's texts, which the converter fills in every
language), held equal to the converter's by
tests/100-layouts-match-converter.lua; they stay unmarked there, as
Map's LoadingScreenID does (decision 180), so a narrower AreaTable
copy isn't refused whole for a name: this checks the width itself.
The converter's own table list marks its AreaTable layout unchecked
against the original's, and what the original does with an area beyond
naming it (its light, music, sub-zones) is recalled: nothing here uses
more than the name.

Window-free (CORE: the test program and the executable). AreaHere is
a small record of the tile last read, kept by its caller.

## What it offers

- `type AreaRow`
  An AreaTable row:
    id         its ID
    continent  its ContinentID (the map it is on)
    parent     its ParentAreaID (0: a zone of its own)
    name       its AreaName, the first (English) text
    from       where the row came from (074's place)
- `WErr area_find(Tables *tables, uint32_t id, AreaRow *out, WWhy *why);`
  AreaTable row `id`. W_OK with `*out`; otherwise `*out` is cleared and:
    W_ERR_NOT_FOUND  id 0, no row of that number, or a copy too narrow
                     for AreaName (each said)
    074's refusals   a copy 074 refuses
- `int area_chunk_at(const AdtArea cells[256], double x, double y);`
  Which of a tile's chunks (adt_read_areas's order) holds world point
  (x, y): its index, or -1 when none does (the point is off the tile).
- `type AreaHere`
  Where she is, kept between calls:
    tx, ty     the tile whose chunks are held (-1, -1: none yet)
    tile_read  1: its chunks were read; 0: no ground there (why says)
    cells      those chunks' corners and areas
    id         the area she stands in (0: none: off the ground, or the
               chunk names none)
    named      1: AreaTable has that row (row holds it); 0: why says
    why        the reason when tile_read or named is 0
- `void area_here_init(AreaHere *a);`
- `int area_here_update(AreaHere *a, Resolver *r, MpqChain *chain, Tables *tables, const char *map_name, double x, double y);`
  Her place now on map folder `map_name`: reads the tile's chunks when
  she has entered another tile (through `r`, or the archives alone,
  `chain`), and AreaTable's row when the area changes. Returns 1 when
  the area (id, or whether it is named) changed, else 0.
