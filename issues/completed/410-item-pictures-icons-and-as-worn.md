# 410 — Item Pictures: Each Item's Icon, and the Item As Worn

*Asked for by wow-chat-2026 (its issue 718a, 2026-10-23): its page of
the tabards a death knight's oath accepts needs pictures. The owner
there: "I will need icons on the tabard HTML page. Maybe also a render
of the tabard as it's being worn?" That project has no copy of the
game's art; this client reads it and already dresses a character, so
the pictures are made here and that project's page generator embeds
them.*

## Status

- Phase: 4 (built on the character and its equipment)
- Builds on: [404](../404-characters-and-equipment.md) (a character
  dressed in its items), [107](../107-blp-parser.md) (the game's
  pictures read)
- Serves: wow-chat-2026 718a (the oath tabards page), and any later page
  that shows items
- State: completed 2026-10-23

## Current behavior

Built (2026-10-23), steps 1 to 5:
- **`scripts/item-pictures LIST [OUT_DIR]`** runs the dressing room
  (`src/058-dressing-room.c`, built by `scripts/build` as
  `dressing-room`). All 87 tabards in the world database take about ten
  seconds: 87 icons, 172 worn views, 1 refused.
- **Icons**: the item-look table's field 6 names the icon
  (`character_item_icon` in 025-character), read and decoded, saved
  64 x 64.
- **As worn**: the night elf woman, a fresh hunter, **hair style 3** (an
  updo; see Answered), wearing the item over her outfit, standing, drawn
  front and back into a 512 x 640 picture held on the graphics card,
  see-through round her, one fixed light. The window is hidden and lends
  only the card, so picture size doesn't depend on the window manager.
  Options choose race, sex, class, hair style and colour.
- **Refusals**: a line that isn't `NAME SLOT:LOOK`, a name that isn't
  safe in a file name, a look with no icon, or an outfit with anything
  missing from the archives (counted by the character as
  `items_unpainted`, by the figure as `items_missing`, attached items
  left off included) is named and refused, and the run exits 1. The one
  tabard refused, "Unused Tabard of Chow" (look 3864), names two torso
  pictures no archive holds; the oath page leaves "Unused" tabards off
  anyway.
- **Receipt**: `output/goodbye`, written last: when, list, folder,
  wearer, lines, icons, worn views, refused, seconds.
- **Shared with the client** (moved, not copied): the figure shader
  (`figure_shader`, 041-figure) and the `--wear` text reader
  (`character_wear_text`, 025-character).
- **Tests** (044-test-client): the Stormwind tabard's icon is in the
  archives and is 64 x 64; an unknown look has no icon; the Stormwind
  tabard is worn whole and the Tabard of Chow's two missing pictures are
  counted. 224 checks pass.
- `scripts/generate-info` now also writes a page for each program file
  (a `.c` with no header) from its opening comment: the dressing room's,
  and the client main's, which had none.

Seen in the pictures: from behind, her default long hair (style 0)
covered the tabard's upper back. Style 6, the other updo, lays a braid
over the front crest. Style 3 leaves both sides clear. All nine oath
tabards show their side's crest.

Handed over (step 6, 2026-10-23): the owner was told the list's shape, the command and where the 87 tabards' pictures are, to pass to the session working on wow-chat-2026's 718a.

How it was before: the client could wear one item for one `--shot`, but
each picture loaded Ashenvale (about 34 seconds), took its size from the
window manager (885 x 1410 here, not the 1280 x 720 asked for), and was
read off the window. One try in three came back all black.

## Intended behavior

**One program, the dressing room, makes both pictures for a list of
item looks in one run.** It is given a list (one line per picture: a
name for the files, the slot, the item look) and a folder, and for each
line writes:

- `NAME-icon.png`: the item's icon as the game stores it (64 x 64),
  read through the item-look table; no window is needed for this part;
- `NAME-front.png` and `NAME-back.png`: her (the client's night elf, in
  her fresh hunter's outfit) wearing the item, standing in her standing
  pose, seen from the front and from behind, on a plain backdrop, at a
  fixed size (512 x 640), lit by a fixed light. The pictures are drawn
  into an off-screen picture on the graphics card (a render texture),
  not read off the window, so the window's size, its place on the
  screen, and whether anyone can see it don't change them.

The world is not loaded: no ground, no buildings, no creatures. One
start of the program serves the whole list, so 81 looks take seconds
of drawing and not minutes of world-loading.

**It says goodbye with a receipt.** Its last act is writing
`output/goodbye` with when it ended and the counts that prove its work
(lines read, icons written, worn pictures written, lines refused and
why). A line it can't make (no such item look, no icon in the
archives) is named on the terminal and counted, and the run ends with
a failing exit status if any were refused: a page missing a picture
should be noticed, not filled with a stand-in.

**Data making and data showing stay apart.** This program only makes
pictures. Which items to picture, and how a page shows them, belong to
whoever asks (for 718a, wow-chat-2026's tabard page generator writes
the list from its world database and embeds the pictures).

## Suggested implementation steps

1. **The icon's name from an item look**, beside the other item-look
   readings in the character file (025-character): the item-look
   table's row for the look, its first icon name (field 6), the path
   `Interface\Icons\NAME.blp`. Fails with a reason for a look not in
   the table or a row with no icon. Test (044-test-client): the
   Stormwind tabard's look (59981) names an icon that is in the
   archives and decodes to 64 x 64.
2. **The figure's shaders shared.** The bending and lighting programs
   the graphics card runs for every figure (now constants inside the
   client's main file) move into the figure file (041-figure), with a
   call that loads them, so the client and the dressing room draw
   figures the same way.
3. **The dressing room** (`src/058-dressing-room.c`): a hidden window
   for the graphics card's sake; the archives opened; per line, her
   look with the item laid over her starting outfit, built as a figure,
   posed standing, drawn into the off-screen picture from the front and
   from behind, saved; the icon decoded and saved. Built by
   `scripts/build` as its own program.
4. **`scripts/item-pictures`**: runs it. Takes the list file, the
   output folder (default `tmp/shared-memory/item-pictures/`), and the
   client's Data folder as `scripts/run` does.
5. **Checked by eye and by receipt**: the four oath tabards that matter
   most (Stormwind's, Orgrimmar's, both Tabards of Conquest) made and
   looked at; then the whole list of 81; the receipt's counts match the
   list.
6. **Handed over**: wow-chat-2026's 718a told where the pictures are and
   what the list file's lines look like.

## Answered

- **Her hair** (2026-10-23): the first back views showed her long hair
  over the tabard. The owner: "pick a model with an updo." Each of the
  night elf woman's styles was drawn from behind. Style 3 (swept up,
  neck bare) became the default; style 6 was passed over because its
  braid crosses the front crest.

- **The wearer** (2026-10-23): asked whether the tabards should be worn
  by a death knight, or by one wearer per side. The owner: "it's fine,
  night elf hunter works for now." `--race`, `--sex` and `--class`
  choose another wearer when one is wanted.
- **The views** (2026-10-23): asked whether to add a three-quarter view.
  The owner: "nah." Front and back only.
- **The tabard's lower piece** (2026-10-23): this client always shows the
  tabard's long piece (1202). The original picks the piece by the
  tabard's first group number (1201 + n), so short tabards may be drawn
  too long. The owner: "this will be fixed naturally when the client
  improves." The pictures follow 404's piece rules as those are built;
  nothing separate is done here.
- **The receipt** (2026-10-23): `output/goodbye` is rewritten on every
  run, so it is git-ignored. The owner: "ignored. the .gitignore file is
  for build artifacts."

## Related

- [404](../404-characters-and-equipment.md): how an item is worn.
- [110f](../110f-views-drawn-into-textures.md): the client's own views
  drawn into textures; the dressing room's off-screen picture is the
  same idea on a small scale.
- wow-chat-2026 `issues/718a-allegiance-after-the-story.md` and its
  `scripts/generate-oath-tabards-page`.
