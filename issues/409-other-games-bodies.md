# 409 — Another Game's Bodies: Playing as a Final Fantasy XIV Character

*Came from: the owner's unsorted note in the project root
(`new-issue-please-sort`, 2026-10-13), sorted 2026-10-16. The owner on
sorting it: "ugh yes even though that's a bad idea right now..." — so it
waits behind the rest of Phase 4.*

The note, as written:

> hi can we add the option to load FFXIV character models and play as one
> of those characters?
> but, we'd need models for each of the different items you can equip, so...
> we'll have to sort them roughly by name. It's okay if they seem a little
> weird.

## Blocked by

- [401](./401-m2-reader.md), [402](./402-skeletons-and-animation.md),
  [404](./404-characters-and-equipment.md): the client's own characters,
  animation and equipment have to be finished first, because this issue
  swaps what they draw. Equipment rules, sheathing and creature displays
  are still unfinished in 404.

## Current behavior

None of this exists. Every character is drawn from the original WoW
client's files: a race and sex's M2 model, its painted skin sheet, and the
equipment the server names (404).

## Intended behavior

A player can choose, on their own side only, to see their character (and
perhaps everyone's) as a Final Fantasy XIV body, read in place from a
Final Fantasy XIV installation the player owns.

- **It's only a different look.** The server still sends only numbers: a
  race, a sex, and item display ids (404, answer 2 of wow-chat-2026 173d:
  each client turns ids into looks from its own assets). Another player
  without this option sees an ordinary WoW character. Nothing reaches the
  server, and other players are unaffected.
- **Gameplay doesn't change.** Collision size, reach, missile height and hit
  timing come from the shared criteria set for the WoW race
  ([012](../docs/012-asset-replacement-and-provenance.md), "Userspace tools
  and the shared criteria set"), never from the borrowed body. The body is
  scaled to the WoW race's height.
- **Each WoW item maps to a Final Fantasy XIV item, roughly by name.** A
  WoW "Leather Boots" becomes whichever Final Fantasy XIV foot item's name
  is closest. Odd matches are accepted ("It's okay if they seem a little
  weird"). The matching is a generated table, made by a tool and kept so
  it can be read and corrected, not a guess made every frame.
- **Through the resolver.** Every borrowed piece is counted under its own
  source (a new source name beside "pack", "blizzard" and "placeholder"),
  so the phase demos can show how much of a picture came from which game.
- **Never shipped.** Like Blizzard's files, Final Fantasy XIV's are read
  from the player's own installation and never copied into this project.

## What has to be built

Listed by the machinery each needs. The Final Fantasy XIV format facts are
what the modding community publishes. Check each one against the
community's format notes before building.

1. **The archive reader.** Final Fantasy XIV keeps its files in "SqPack"
   archives (paired index and data files), looked up by a hash of each
   file's path. This is a second archive chain beside our MPQ reader (105),
   behind the same file-access layer (104).
2. **The table reader.** Item names, slots and model numbers live in its
   "EXD/EXH" sheets (header, data, per-language name files). This is a
   second table reader beside our DBC reader (106).
3. **Models and pictures.** Its own model format (`.mdl`, several meshes
   and levels of detail), materials (`.mtrl`) and textures (`.tex`). The
   textures use the same block-compression family our card already draws
   (204).
4. **Skeletons and animations — the hard part.** Its skeletons (`.sklb`)
   and animations (`.pap`) wrap data in a format from Havok, a commercial
   physics and animation library. Reading Havok's compressed animation
   tracks is the riskiest step in this issue. Find out what the community
   decoders do before choosing a route.
5. **Animation by name.** WoW's animation numbers (stand, walk, run,
   backpedal, jump start, loop and land, attack, cast, emotes) mapped to
   Final Fantasy XIV's animation names, in a generated table like the item
   table. Where it has nothing to match, the body stands still and the
   terminal says so (a fallback, so it is warned about).
6. **Attachment points.** WoW's attachment points (hands, back, hips,
   shield) mapped to Final Fantasy XIV bone names, so weapons sit in the
   right hand.
7. **The item-matching tool.** Reads both games' item names and slots and
   writes the WoW-item to Final-Fantasy-XIV-item table, with a closeness
   score per row. It uses the same "make the tool, not the thing" rule as
   everything else. WoW has more slots (shoulders, waist, wrists, cloak,
   tabard) than Final Fantasy XIV's five armour slots. Those either merge
   into a neighbour or show nothing (see the open questions).
8. **The choice.** A setting in `Config.wtf` and the options menu (1001):
   off, my character only, or everyone.

## Suggested implementation steps

1. Read the community's format notes for SqPack, EXD, `.mdl`, `.tex` and
   the Havok-wrapped skeletons and animations. Write what is known into a
   format document beside [003](../docs/003-asset-formats.md).
2. The SqPack reader behind the file-access layer, with a test that lists a
   known file.
3. The EXD reader, with a test that prints a handful of item names.
4. One body, unanimated, drawn in place of a WoW character at the WoW
   race's height.
5. Skeleton and one animation (standing), then the animation-by-name table.
6. The item-matching tool and its table. Equipment drawn from it.
7. The setting and the menu entry. A phase demo picture with the
   resolver's counts by source.

## Open questions

1. **Whose bodies change?** Only the player's own character, every
   player's, or creatures too (WoW humanoid NPCs are dressed characters,
   404)?
2. **Which Final Fantasy XIV race stands in for which WoW race?** By
   build (a tauren as a Roegadyn, a gnome as a Lalafell), or chosen by the
   player?
3. **WoW slots with no Final Fantasy XIV counterpart** (shoulders, waist,
   wrists, cloak, tabard): merge into the nearest slot, or leave them
   unshown?
4. **Havok.** If reading the animations needs Havok's own software or
   reverse-engineering it, is an unanimated (or WoW-skeleton-driven) body
   acceptable as a first step?
