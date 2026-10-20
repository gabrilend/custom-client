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
5a. Wrapping and cutting as the original does them (clean-room
   checklist, [docs/057](../docs/057-what-the-original-program-decides.md)
   note 1.9 and row "text that doesn't fit", filled 2026-10-16):
   - a line's width counts kerning, the outline (2 or 4 pixels), the
     shadow offset and embedded textures; colour and link codes count
     nothing;
   - breaks allowed after `-`, `;`, `/`, before `|` and spaces, and
     between CJK characters; never after an opening mark or before a
     closing one (both lists in note 1.9); the last allowed break that
     fits, else mid-word; spaces at a new line's start skipped;
   - cutting only for a string with a width and a line count or height:
     whole UTF-8 characters removed from the end until the text plus
     three full stops `...` fits; text over 8188 bytes cut first.
   Codes inside text (note 7.7, 7.8): `|cAARRGGBB` colours, `|r`, `|n`,
   links `|H…|h…|h` (capital H only), pictures `|T…|t` (capital T only)
   with their size fields, `||` as one `|`; a code that doesn't read
   correctly draws the `|` as a character; each kind can be switched
   off per string. One **quirk**: a colour's alpha pair is read but
   ignored, so text is always opaque; its correction switch (off by
   default) uses the alpha.
   Tests: "one-two" breaks after the hyphen; "(word" never breaks after
   the bracket; Japanese text breaks between characters; a two-line box
   holding three lines ends in `...` on its second line; `|cff00ff00`
   turns text green; `|cZZ00ff00` draws its `|`; `|h` with a lower-case
   start is not a link; `|T…:0:2|t` is twice as wide as the text is
   tall; `|c80ff0000` is opaque red, and half see-through with the
   correction on.
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

## Blocks

- [804](./804-floating-combat-text.md).
