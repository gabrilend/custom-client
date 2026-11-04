# 073-converted-map

*Written by `scripts/generate-info` from `073-converted-map.h`; don't edit, re-run it.*

073-converted-map.h (issue 1109, open question 4; world-edit-to-execute
W02c) - which map a converted build folder holds, and where to start

For a general audience: world-edit-to-execute's converter turns a
Warcraft III map into this game's files in a build folder: the ground
(World\Maps\<folder>\...), the map's rows in the client's tables
(DBFilesClient\), its creatures (world\creatures-<id>.tsv), and a
small file, map.lua, saying which map the folder holds: its number,
its folder under World\Maps, its name, where the first player starts,
and the rule that turns Warcraft III's positions into this game's.
A test-map run (067-test-map.h) reads it to know which map to open and
where to put her, instead of opening Kalimdor.

map.lua, format 1, as W02c writes it (src/wowbuild/init.lua there); a
Lua table of plain data, read with 059's data reader (no Lua runs):

  return {
      format = 1,
      map = { id = 900, directory = "WC3_Master_Crafters_Final_Conquest",
              name = "Master Crafters Final Conquest", area = 9000, slot = 0, ... },
      start = { x = -495.833, y = -441.667, z = 4.16667, player = 0, wc3 = { 13568, -15232 } },
      wc3_to_wow = { scale = 0.0325521, x0 = 0, y0 = 0, rule = "..." },
      creatures = "world/creatures-900.tsv",
      tiles = { first = { 31, 31 }, across = 2, down = 2 },
      maps = { ... },                -- every map of the folder (not read here)
  }

Read here: format (1; any other is refused), map.id (a whole number, 1
to 65535: a map's number is 16 bits on the server), map.directory (1
to 48 letters, digits and _, as the converter makes it; never a path),
map.name (optional), map.area (optional), start.x and start.y (yards,
required), start.z (optional), wc3_to_wow.scale (yards per Warcraft
III unit, above 0), .x0, .y0 (yards), creatures (optional: a path
inside the folder, never leaving it). Fields not named here are passed
over, so the converter's later parts can add their own (decision in
1109).

Window-free: in the test program and the executable.

## What it offers

- `type ConvertedMap`
  What map.lua says (plain values; nothing to free).
- `WErr converted_map_read(const char *folder, ConvertedMap *out, WWhy *why);`
  Reads `folder`/map.lua into `*out`.
    W_OK
    W_ERR_NOT_FOUND  the folder has no map.lua (not a converted build
                     folder, or one written before W02c)
    W_ERR_CORRUPT    not Lua data (with its line)
    W_ERR_UNSUPPORTED  a format other than 1
    W_ERR_REFUSED    a field missing, of the wrong type or out of its
                     range (named, with its line); a file too large
    W_ERR_IO         it can't be read
- `void converted_map_from_wc3(const ConvertedMap *m, double wc3_x, double wc3_y, double *x, double *y);`
  A Warcraft III position (units) as world yards, by the map's rule.
