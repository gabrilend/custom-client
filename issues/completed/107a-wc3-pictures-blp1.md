# 107a — WC3's Pictures (BLP1)

*Parent: [107](../107-blp-parser.md). The first step of the second
crosswalk ([docs/009](../../docs/009-roadmap.md), "world-edit-to-execute's
client arriving"). C twin of world-edit-to-execute's
`src/parsers/blp.lua` and `src/parsers/jpeg.lua` (its issue 522a).*

## Current behavior

Built 2026-10-07. `blp_decode` reads WC3's pictures as well as the
game's, telling the two versions apart by their first four bytes, into
the same `BlpImage`; `blp_decode_level` reads any smaller copy. Both are
in the reader library, so world-edit-to-execute can call them from Lua.

- `src/049-jpeg.c` / `.h`: a JPEG reader of the client's own. Markers,
  quantization and Huffman tables, baseline and progressive scans (DC
  and AC, first passes and refinements), restart intervals, parts at any
  sampling widened by repeating samples. Four parts are handed back as
  stored (BLP1's blue, green, red, alpha); three are turned from YCbCr
  into red, green, blue. Its state lives in one structure on the heap for
  one decode, so threads may decode at once. Refuses with a code and a
  sentence: lossless, hierarchical and arithmetic-coded frames, 12-bit
  samples, more than four parts, a Huffman code no table holds, a scan
  or segment past the end. It never reads past the bytes it was given.
- `src/017-blp.c`: BLP1's two encodings. JPEG: the header shared by
  every size and the level's own remainder are joined and decoded; only
  the level's size is kept. Palette: one colour number a pixel, then
  alpha at 0, 1, 4 or 8 bits a pixel (shared with BLP2's palette
  decoding). Every level's bytes are checked to lie inside the file, for
  BLP2 too, which before read whatever its table said.
- `scripts/build`: the JPEG reader joins the reader library (which now
  links the maths library).

Checked:
- `tests/044-test-client.c`, "WC3's pictures (BLP1)": a 2 x 2 palette
  picture made in memory decodes to its known colours and alphas; its
  level 1 to one pixel; a missing level is not-found; a file cut short is
  corrupt; a stray JPEG is corrupt; WC3's `war3.mpq` opens with the
  client's own archive reader (015); the footman's picture
  (`Textures\Footman.blp`, a JPEG BLP1) decodes to 256 x 256 with the
  same bytes as the Lua reader (a checksum taken from the Lua).
- `tests/050-blp1-matches-lua.lua` (LuaJIT, split over every core):
  every BLP1 in `war3.mpq` and `War3x.mpq`, levels 0 and 1, through both
  readers. 2026-10-07: 12942 decodes, 12680 identical, 262 refused by
  both (all level 1 of pictures with no smaller copy), none disagreeing.
- The test program built with the address and undefined-behaviour
  checkers: clean.

## Intended behavior

Any picture WC3 uses decodes in the W client to the same pixels
world-edit-to-execute's Lua reader gives, so WC3 models (401 step 2)
can be painted with them, and either project can stand in for the
other's picture reading.

## Suggested implementation steps

1. Port the JPEG reader step for step: the same cosine table built the
   same way (Lua's value of pi), the same order of additions in the
   transform, `floor(s + 128.5)` and clamping, colour widening by
   `floor(x / (hmax / h))` in doubles, colour conversion's `+ 0.5`
   truncated. Byte-identical output depends on these.
2. Bounds: every header read checks the end; the bit reader treats the
   end of the bytes as a marker (zeros after), as the standard asks.
3. BLP1 in the picture reader beside BLP2, by magic; a level argument.
4. The two checks above.

## Decisions

- **Our own JPEG reader, not the system's libjpeg.** libjpeg smooths
  widened colour, so its pixels differ from the Lua reader's; owning the
  reader keeps both projects on one answer and matches the client's
  own-archive-reader choice (docs/002).
- **Restart intervals.** At an interval's end this reader steps over the
  restart marker whether or not the bit reader had already met it. The
  Lua twin steps over it only when it had, so on a JPEG with restart
  intervals the Lua would read every other interval as zeros. No WC3
  picture has restart intervals (the full parity run agrees on all), so
  the two agree on every picture they are used for. The Lua side is
  world-edit-to-execute's to fix; reported to the owner 2026-10-07.

## Related

- [107](../107-blp-parser.md) (the parent: BLP writing remains there).
- Blocks: [401](../401-m2-reader.md) step 2 (WC3 models' pictures).
- world-edit-to-execute `docs/wow-client-bridge.md`, "The client's move
  into the W client".
