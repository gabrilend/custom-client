# 404 — Characters, Their Equipment, and the World's Creatures

*Came from: wow-chat-2026 173d (models), 173p (equipment on the body),
173j (figures standing around, completed there) and 173t (creatures from
the world database), moved 2026-10-07.*

## Blocks

- [409](./409-other-games-bodies.md): another game's bodies drawn in place
  of these characters.
- [811](./811-spell-visuals.md).

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

Measured against the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md), section 5,
filled 2026-10-16):
- **Already the original's:** the skin sheet's ten areas and their
  rectangles (note 5.1; this client's table for 256 pixels, doubled,
  is the same as the original's for 512).
- **Not yet the original's** (note 5.2):
  - item pictures are painted in the order of the slots here; the
    original stacks each area by a **layer number** per slot and area
    (shirt 0, chest 1, legs 0, boots 2, wrists 2, gloves 3, tabard 4,
    belt 5 on the lower torso and 2 on the upper leg), lowest first;
  - those numbers move with the item's mesh groups: sleeves to 5, cuffed
    gloves to 6, a robe's skirt to 4, flared legs to 3 or 4, tall boots
    to 3; taking off a robe puts the legs and boots back;
  - underwear is always painted here; the original shows it on the upper
    torso only when layers 0 to 2 are empty, and on the upper leg when
    layers 0 and 1 are empty or the chest covers the legs (item display
    flag 4, which also limits what shows on the legs);
  - the face here is face then hair; the original paints the skin under
    it only when the skin's record has flag 8, then face, then **facial
    hair** (not painted here at all), then hair;
  - a race with race-table flag 2 never gets a foot picture from boots.
- **Mesh pieces** (which item shows which mesh group): this client's
  own reading, in the character file's item-wearing step, against the
  original's rule (note 5.6, filled 2026-10-16). Each item display has
  three group numbers (first, second, third):
  - **matching:** gloves' first group (area 4), boots' first group
    (piece 501 + *n*), a chest's first group as sleeves (801 + *n*), a
    robe's third group (1301 + *n*), legs' third group (1301 + *n*),
    belt (area 18), cloak (area 15);
  - **swapped:** legs' first group is shown here in area 9 and their
    second in area 11; the original does the reverse (first group →
    1101 + *n*, second → 901 + *n*);
  - **missing:** the order of the original's nine steps, where later
    steps override earlier ones (gloves win over sleeves; a robe hides
    areas 5, 9 except 901, 11 and 13; legs' dress yields when the chest
    covers the legs); boots also showing 901 and hiding area 5; a shirt's
    sleeves only when nothing covers the lower arm in picture layers 1
    to 6; a shirt's second group only without a tabard; the tabard by
    its first group (1201 + *n*) rather than always piece 1202; the
    death knight's glowing eyes (1703); everything hidden from 0 to
    2000 at the start except piece 0 and the 19 chosen pieces;
  - an unidentified character flag in the original also shows 1201
    (and 1202 without a robe or dress): not built until the flag is
    named (answered question 2).
- **Weapons** (row "weapons sit at these attachment points", filled
  2026-10-16): in hand, the main hand on attachment 1, the off hand on
  2 or the shield point 0; guns, crossbows, wands and thrown weapons in
  the right hand, bows in the left; a holding hand's finger bones (8 to
  12 right, 13 to 17 left) take a grip pose until the item is removed.
  Sheathed by the item's sheath type: 1 on 26 or 27, 2 (back) on 30 or
  31, 3 (hip) on 32 or 33, 4 (shield) on 28. The server sends the sheath
  state; the client draws a ranged or hand weapon itself for spells
  that use it. Not built here: sheathing at all, and the grip pose.
- **Riding** (note 5.7): not built. The mount is its own model from the
  mount display (its pieces, skin and scale); the rider hangs on the
  mount's attachment 0, scaled by 1 ÷ the mount's scale so the rider
  keeps their size, playing the riding pose the mount's animation
  behaviour names. The mount takes the rider's colour tint; footprints
  and sounds come from the mount; the body grows ([802](./802-collision.md)),
  dismounting at once if there's no room; the camera refits.
- **Missiles leave** the caster's `$CSL`, then `$CSR`, then `$CST` model
  event, else the unit's position (note 5.5): for spell visuals, when
  they come.

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

1. The original's stacking (docs/057 note 5.2; link back when done):
   each area built as skin, then underwear where it shows, then item
   pictures by layer number, with the mesh-group moves, the robe's
   covering of the legs, the face's four layers with facial hair, and
   the footless races. Small pictures stretched to their rectangle;
   large ones read from the stored copy that matches the width (note
   5.1).
   Mesh pieces by note 5.6's nine steps in order, built together with
   the picture layers, since the same group numbers drive both.
2. Tests: legs with a first group of 1 show piece 1102 and not 902;
   gloves with cuffs hide a chest's sleeves; a robe hides the boots'
   piece; a death knight shows piece 1703. For the pictures: cuffed
   gloves over a sleeved chest over bracers on the lower
   arm; a shirt under a chest; underwear gone on the upper torso under a
   shirt and back when it's taken off; a robe hides the belt on the
   upper leg; boots leave the feet bare on each race whose race-table
   flags include 2 (which races those are is read from the table, not
   assumed).
3. Weapons in hand and sheathed by the attachment numbers above, the
   grip pose, the server's sheath state; items' animations and
   particles. Tests: a bow sits in the left hand and a gun in the right;
   a sheathed two-handed sword with sheath type 2 hangs on 30.
3a. Riding (note 5.7): a mount and rider pair, the rider's scale
   making up for the mount's, the riding pose, the dismount when the
   taller body won't fit. Test: a rider on a mount scaled 2.0 is drawn
   at their usual size.
4. Creature displays' own pieces; creatures' weapons.
5. Creatures from the server (with Phase 6's world entry).

## Open questions

1. A townsperson whose facial-hair choice lies beyond the table (eleven
   Astranaar sentinels: choice 10) is drawn with the race's choice-0
   pieces, a fallback said on the terminal. What does the original show
   for them? (docs/046, check 13.)

## Answered

2. ~~**The unidentified flag.**~~ Not built until it's known
   (2026-10-20). docs/057 note 5.6 step 6: a character flag the
   describer couldn't identify also shows pieces 1201 (and 1202 without
   a robe or dress), the pieces a tabard shows. Asked whether to build
   it behind a guess, the owner: "We need to find the answer to that
   question, or leave it unknown until we know." So no guess is built:
   a character shows those pieces only for a tabard. Two ways to learn
   which flag it is, neither of them a builder reading the
   decompilation:
   - the finished docs/057 names it. Nobody asks the describer to: the
     describer works uninterrupted and unsteered, and builders use what
     the finished checklist gives (the owner, 2026-10-20);
   - watching the original 3.3.5a client against the project's
     AzerothCore server while the server sets a character's flags one at
     a time, and seeing which one makes the tabard pieces appear with no
     tabard worn. That observes the program from outside, so it stays
     clean-room.

## Related

- [402](./402-skeletons-and-animation.md), docs/046.
