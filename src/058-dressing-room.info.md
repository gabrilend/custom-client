# 058-dressing-room

*Written by `scripts/generate-info` from `058-dressing-room.c`'s opening comment (a program: it offers no functions, only how it is run); don't edit, re-run it.*

058-dressing-room.c (issue 410) - pictures of items: each item's icon,
and a character wearing it, for pages outside the game

For a general audience: a page about the game's items (wow-chat-2026's
list of the tabards a death knight's oath accepts was the first) needs
pictures of them, and only the game's own files hold those. This
program is given a list of items and writes, for each, the item's icon
as the game shows it in bags, and a picture of a character wearing it,
from the front and from behind. No world is loaded: the character
stands alone on a see-through backdrop, under one fixed light, drawn
into a picture held on the graphics card (not read off the screen), so
every picture comes out the same size whatever the window manager does
with the window, which stays hidden.

Usage: dressing-room --client DIR --list FILE --out DIR [--receipt FILE]
                     [--race ID] [--sex 0|1] [--class ID] [--hair N] [--hair-color N]
  --client   the original 3.3.5a client's Data folder
  --list     one picture per line: NAME SLOT:LOOK (NAME the files'
             name, letters, digits, - and _; SLOT a slot as the
             client's --wear names it; LOOK an item look, an
             ItemDisplayInfo id). Blank lines and lines starting with
             # are skipped.
  --out      the folder the pictures go to: NAME-icon.png (64 x 64),
             NAME-front.png and NAME-back.png (WIDTH x HEIGHT below)
  --receipt  where the goodbye receipt goes (default: none written)
  --hair, --hair-color  her hair style and colour (creation choices,
             from 0; default style 3, an updo that leaves a tabard's back
             in view, colour 0)
  --race, --sex, --class  who wears the items (ChrRaces id, 0 male or
             1 female, ChrClasses id for the starting outfit worn under
             them); the client's night elf woman, a fresh hunter, by
             default

Exits 0 when every line made all three pictures; 1 when any line was
refused (each named on the terminal and counted in the receipt): a page
missing a picture should be noticed, not filled with a stand-in.
