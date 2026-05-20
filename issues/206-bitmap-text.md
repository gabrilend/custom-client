# 206 — Bitmap Text

## Current behavior

raylib has `DrawText` and TTF loading via `LoadFontEx`. That covers
generic on-screen text, and we will use it for the Phase 2 HUD. What
raylib does **not** give us is the original game's fonts: Blizzard
shipped Friz Quadrata TT, Morpheus, Skurri, Arial Narrow (and locale
variants) as actual TTFs in `Fonts/` inside the MPQ chain. The UI
expects them.

## Intended behavior

A two-tier text system:

1. **Generic text** — uses raylib's `DrawText` / `DrawTextEx` directly,
   with a single bundled debug font for the early HUD and any C-side
   informational text.
2. **Game font** — load Blizzard's TTFs out of the MPQ chain at
   startup, bake into raylib `Font` handles, expose:

```c
err_t  text_load_game_fonts(mpq_chain_t *);

const Font *text_font(text_font_id_t id, int px_size);
void  text_draw(const Font *, const char *utf8, vec2_t pos, vec4_t tint);
vec2_t text_measure(const Font *, const char *utf8);
```

`text_font_id_t` enumerates the half-dozen logical fonts FrameXML
references (`FRIZQT__`, `MORPHEUS`, `SKURRI`, `ARIALN`, locale-CJK,
etc.). The per-size cache means we don't re-bake at every call.

UTF-8 handling matters here — chat strings, character names, addon
text are all UTF-8 (well, with some Blizzard-specific quirks for
emoticons). raylib's text API handles UTF-8 codepoints fine; we just
have to make sure we're never re-encoding it on the way through.

## Suggested implementation steps

1. Add `text.c` to `src/gfx/`.
2. Find Blizzard's TTFs in the MPQ chain. For 3.3.5a-enUS they live at
   `Fonts/FRIZQT__.TTF`, `Fonts/MORPHEUS.TTF`, `Fonts/SKURRI.TTF`,
   `Fonts/ARIALN.TTF`. For other locales the path differs — table-drive
   the lookup.
3. Implement `text_load_game_fonts`: for each known logical id, read
   the TTF bytes, call `LoadFontFromMemory(".ttf", bytes, n, size,
   codepoints, n_codepoints)`. Pre-bake the default codepoint set
   (ASCII + Latin-1 + the WoW-specific glyphs).
4. Cache by `(id, px_size)`. Bake on first request, reuse thereafter.
   Unbake on shutdown.
5. Provide measurement helpers — FrameXML's `FontString` needs to know
   width and height for layout.
6. Tests: render a known string with the debug font, confirm it draws
   without errors; load FRIZQT from a real MPQ chain and measure a
   known string.

## Acceptance

- The Phase 2 demo HUD uses generic raylib text.
- Loading game fonts succeeds against a real MPQ chain.
- A drawn string in FRIZQT 14px matches a reference image within
  per-pixel tolerance (font hinting differences make exact match
  unreliable; structural match is what we assert).

## Stress markers

CJK codepoint ranges are large; baking the full range at every size is
wasteful. v1 bakes the Latin block and a configurable extra range
specified in the config — full CJK support gets its own issue when a
locale that needs it is in scope.

## Related

- [105](./105-mpq-reader.md)
- [docs/006-ui-and-addons.md](../docs/006-ui-and-addons.md)
- Blocked by: [105](./105-mpq-reader.md), [203](./203-shader-pipeline.md).
- Blocks: [208](./208-phase-demo.md), the Phase 6 login UI, Phase 7
  FrameXML.
