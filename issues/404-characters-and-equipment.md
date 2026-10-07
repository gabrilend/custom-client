# 404 — Characters, Their Equipment, and the World's Creatures

*Came from: wow-chat-2026 173d (models), 173p (equipment on the body),
173j (figures standing around, completed there) and 173t (creatures from
the world database), moved 2026-10-07.*

## Current behavior

Built:
- **A character from its look** (`src/025-character.c`): race, sex and
  the creation choices turned into a model with the right pieces shown
  (CharHairGeosets, CharacterFacialHairStyles, every other group's bare
  variant) and a painted skin sheet (CharSections: skin, face, hair,
  underwear), plus fur and hair pictures.
- **Equipment** (ItemDisplayInfo): pictures painted onto the skin
  sheet's regions for the wearer's sex; items switching body pieces
  (sleeves, gloves, boots, robes, belts, tabard); helmets (hiding hair
  and ears as their table says), shoulders, weapons, shields and cloaks
  hung on the model's attachment points. A fresh character's outfit from
  CharStartOutfit.
- **Creatures** (`src/035-creatures.c`, `041-figure.c`): a creature
  display (CreatureDisplayInfo, CreatureModelData, up to three skins) or
  a humanoid display as a dressed character with its baked skin. The
  world's creatures come from a list generated from a world database
  (wow-chat-2026's `scripts/generate-custom-client-creatures`, into
  `assets/world/`), built as they come within 110 yards and freed beyond
  130, each breathing in its standing animation from its own moment.
- A picture the archives lack for one layer of a character is left
  unpainted and named, rather than refusing the character.

Left:
- Items' own animations and particles (glowing weapons); sheathing.
- Creatures' weapons and shields (`creature_equip_template`), sitting and
  emotes (`creature_addon`), walking: with the network.
- Each creature standing at the database's height, not the ground's.
- A creature display's own choice of pieces (its geosets) isn't read: the
  body group and each other group's first variant show.
- Comparing the piece rules with the original on a few sets.
- The world's creatures from the server instead of the generated list
  (Phase 5 and 6), when the generator retires.

## Intended behavior

A character wears what the server says it wears, and every creature
looks as its display says, each turned into looks from the player's own
assets (wow-chat-2026 173d, answer 2: the server sends only ids; each
client turns ids into models and pictures from its own assets).

## Suggested implementation steps

1. Sheathing, and items' animations and particles.
2. Creature displays' own pieces; creatures' weapons.
3. Creatures from the server (with Phase 6's world entry).

## Open questions

1. A townsperson whose facial-hair choice lies beyond the table (eleven
   Astranaar sentinels: choice 10) is drawn with the race's choice-0
   pieces, a fallback said on the terminal. What does the original show
   for them? (docs/046, check 13.)

## Related

- [402](./402-skeletons-and-animation.md), docs/046.
