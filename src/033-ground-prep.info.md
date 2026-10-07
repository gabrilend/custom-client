# 033-ground-prep

*Written by `scripts/generate-info` from `033-ground-prep.h`; don't edit, re-run it.*

033-ground-prep.h (issue 173u) - a tile's ground made ready, without the window

For a general audience: loading a tile's ground has two halves. The
first is arithmetic: read the tile's ground file, work out every point
of its mesh (place, colour, and its light from the server's slopes),
pack its 256 blend maps into one picture, and unpack the ground
pictures it names. The second is handing all that to the graphics card.
Only the second half needs the window's thread; this file is the first
half, so the reading-ahead thread (034-prefetch.h) can do it for tiles she
hasn't reached yet, and the main thread is left only the sending
(038-ground.h). With no window it is also what the tests check.

Every function here is safe on any thread: it touches the archive
reader (locked) and the server's heights (locked where they load), and
nothing else that is shared.

## What it offers

- `type GroundPlace`
  Where tiles are prepared: the archives, the map's folder in them and its
  blend-map form (from its WDT), the server's heights (for the light),
  and the drawing origin points are placed relative to.
- `type GroundQuarter`
  One quarter of a tile (64 chunks of 145 points; 173u draws a quarter
  in one order): the arrays a Raylib mesh takes, filled but for the
  points' four layer numbers (which layer of the card's picture array a
  picture lands in is only known when it is sent).
- `type GroundPixels`
  A decoded ground picture: width x height RGBA, row by row, and its
  smaller copies (each half the last, down to 1 x 1) for the card.
- `type GroundPrep`
  A tile made ready to send.
- `WErr ground_place_init(GroundPlace *p, MpqChain *chain, const char *map_name, MapWorld *world, double ox, double oy, WWhy *why);`
  Reads the map's WDT for its blend-map form and fills `p`. W_OK, or a code
  with `why`.
- `WErr ground_prep_tile(const GroundPlace *p, int tx, int ty, int (*on_card)(const char *name), GroundPrep **out, WWhy *why);`
  Prepares tile (tx, ty). `on_card`, when given, answers whether a
  picture (by archive name) is already on the card; those aren't
  unpacked (sending finds them there, or unpacks them itself if they
  have gone since). W_OK with `*out` (freed by ground_prep_free);
  W_ERR_NOT_FOUND when the map has no ground file there (open sea: not a
  failure, nothing to draw); another code, with `why`, for a fault.
- `void ground_prep_free(GroundPrep *g, int keep_adt);`
  Frees a prepared tile. `keep_adt`: 1 when the ground file was taken
  by the caller (it then frees it with adt_free and free).
- `WErr ground_prep_pixels(MpqChain *chain, const char *name, GroundPixels *px, WWhy *why);`
  Unpacks one ground picture by archive name into `px`, with its smaller
  copies. W_OK, or a code with `why`.
- `void ground_prep_smaller(GroundPixels *px);`
  Makes a square picture's smaller copies (into px->smaller): each pixel
  the average of the four beneath it, as the card itself would make them.
  A picture not square, or whose side isn't a power of two, gets none.
