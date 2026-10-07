# Custom Client: Things to Check In-Game

*Moved here from wow-chat-2026 (its `docs/custom-client-ingame-checks.md`) with the client on 2026-10-07
(wow-chat-2026 issue 173v). Issue numbers like 173u are wow-chat-2026's;
[009](./009-roadmap.md)'s crosswalk says which issue here continues each.*

Things the custom client does by this client's own reading or guess,
which the owner checks against the original client and answers. Each
item names the issue it belongs to. When checked, write the answer into
that issue and strike it here (keep the line, so the history stays).

## Look

1. **Torch flames** (173o): walk up to a torch at night (`--hour 23`).
   Is its flame as tall and as bright as the original's?
2. **Light inside buildings** (173g): inside an Astranaar house, is it as
   bright and as warm as the original's?
3. **Sky bands** (173n): do the sky's colour bands sit at the same
   heights above the horizon as the original's (horizon, two bands,
   middle, overhead)?
4. **Stars and the moon** (173n): at night in Ashenvale, does the
   original show stars or a moon? Its light names no skybox, so if they
   show, they come from elsewhere (the original's own sky drawing).
5. **The sun's path** (173n): at noon and dusk, does light fall from the
   same side as in the original (sun east at 6, south at noon, west at
   18 here)?
6. **Water** (173r): the lake round Astranaar. Its colour,
   see-throughness, and how fast its ripples run (15 pictures a second
   here).
7. **The moss over the archway near the bridge** (173g): is it still
   stretched? Two-picture pieces and moving pictures are now drawn, which
   may have fixed it. If not, where exactly is it (which bridge, or a
   picture)?
7b. **Northrend's aurora** (173n): `--map 571 3694 3440 --hour 23`. Is its
   glow the original's colour and strength?
   **The halos** should now be round from every side (lamps, wisps,
   fireflies): check they look like the original's orbs.
8. **The tauren** (173d, 173p): his tail and coat now show (they used a
   fur picture the client skipped). Do they look like the original's? Are
   his hooves right? Are the two skin layers the client's tables name but
   its files lack (scalp, and a hair picture) visible as patches?

8c. **Starting outfits** (173p): she wears a fresh night-elf hunter's
    clothes and the tauren a fresh warrior's. Do they match a new
    character's in the original? (The axe is held in her hand: the
    original sheathes it on her back out of combat; sheathing isn't
    built.)
8a. **The tauren's beard and hooves** (173d): he wears facial-hair
    choice 1. Does it look like that choice in the original's character
    creation, and are his bare hooves the same?
8b. **Her glowing eyes** (173d): `--front --zoom 2 --hour 23`. Is the
    glow as bright and as large as the original's, by night and by day?
12. **Equipment** (173p): `--wear chest:41143,hands:41138,legs:41140,waist:41476,feet:41480`
    (a rogue's leather set). Do the painted leather, the glove cuffs and
    the tall boots look like the same items on a night elf in the
    original? The piece rules (sleeves, robes, boots, gloves) are this
    client's reading. With `,head:41139,shoulders:41142,mainhand:40857`:
    do the hood, shoulders and sword sit where the original puts them, and
    does the hood hide what the original's hides?

13. **Astranaar's townsfolk** (173t): `--front --zoom 6` or walk into
    town. Do the sentinels, innkeeper and trainers stand where the
    original's do, facing the same way, dressed the same? The sentinels'
    facial-hair choice (10) is beyond the table and drawn with the plain
    pieces: what does the original show?

## Feel

9. **Step height and body width** (173q): can she walk up the same
   ledges, and get as close to walls, as in the original (1 yard and 0.4
   yards here)?
10. **Underwater** (173r): dive in Astranaar's lake (`--goto 2866 -331
    --dive 2`). Is the murk the original's colour, and as close (40 yards
    here)?
10b. **Swimming** (173r):
   - How deep the water gets before she swims (1.6 yards over her feet
     here).
   - How fast she floats up when left alone (2 yards a second here).
   - Swimming forward with the right button held follows the camera's
     pitch.
10c. **Buildings' pools** (173r): the basin in a small night-elf house's
    garden at Astranaar (`--goto 2796 -320.6 --pitch 40`), and the pool
    in the Timbermaw cave (3257, -660). Is the water where the
    original's is, and is it the same colour as the lake's?
10d. **Water by depth** (173r): Astranaar's lake (`--goto 2800 -420
    --face -60 --zoom 14 --pitch 35`). Does it go from see-through at
    the shore to opaque as quickly as the original's, and darken the same?
11. **Camera follow speed** (173m): the swing behind her at 180 degrees a
    second.
11b. **First person and Head Bob** (173m): roll the wheel in past the
    nearest (2 yards) and the view goes to her eyes, she hidden.
   - Is the eye height right (92 % of her height here)?
   - With Head Bob on (`SET cameraBobbing "1"` in the settings file), is
     the bob as high (0.05 yards here) and once a step (every 1.4 yards
     here) as in the original?
   - The original's first person: does it allow looking straight up, as
     this one does?

## How to look

`scripts/run` runs it. Picture options (after `--shot`):
`--hour H`, `--face DEG`, `--walk S`, `--zoom N`, `--pitch DEG`,
`--turn DEG`, `--pose ID MS`, `--clock MS`, `--goto X Y`, `--map ID X Y`.
