# 025-character

*Written by `scripts/generate-info` from `025-character.h`; don't edit, re-run it.*

025-character.h (issue 173k; the C twin of lib/character.lua) - one player
character, put together

For a general audience: a race's model holds every hair style and
clothing shape at once, and its skin is painted from several pictures
laid onto one sheet (body, face, scalp, underwear). The game picks which
pieces show and which pictures go where from its own tables
(CharHairGeosets, CharacterFacialHairStyles, CharSections); this does
the same for one character's choices, and dresses it in its items
(173p).

## What it offers

- `type CharacterLook`
  the slots a character wears things in (173p), in the order they are
  painted onto the skin sheet when two cover the same region: what lies
  under is painted first
- `type CharacterAttachment`
  an item model hung from one of the character's attachment points (173p):
  model and picture archive paths, and the point (M2 attachment id: 1
  right hand, 2 left hand, 0 shield, 5 right shoulder, 6 left shoulder,
  11 helm)
- `type Character`
- `WErr character_start_outfit(MpqChain *archives, int race, int class_id, int sex, int items[SLOT_COUNT], WWhy *why);`
  A fresh character's clothes and weapons: the client's starting-outfit
  table (CharStartOutfit.dbc) row for race, class (ChrClasses id: 1
  warrior, 3 hunter, 11 druid ...) and sex, its items' looks put into
  `items` by the kind of slot each fills. Slots it doesn't fill are left
  as they were. Ranged weapons (bows, guns, thrown), ammunition, bags and
  food have no slot here and are left out. W_OK, or a code with `why` (no such
  row).
- `int character_wear_text(int items[SLOT_COUNT], const char *text, char *bad, int bad_len);`
  Items to wear, written as text (410; the client's --wear): "slot:look"
  pairs joined by commas, e.g. "chest:41143,hands:41138", each look (an
  ItemDisplayInfo id) put into `items` at its slot. Slot names: shirt
  legs feet wrists chest hands tabard waist head shoulders back mainhand
  offhand. 0, or 1 with the part it couldn't read copied into `bad`.
- `WErr character_item_icon(MpqChain *archives, int look, char *path, size_t path_len, WWhy *why);`
  An item look's icon (410): the archive path of the picture the game
  shows for the item in bags and on the character pane
  (Interface\Icons\<name>.blp, from ItemDisplayInfo), written into
  `path`. W_OK, or W_ERR_NOT_FOUND with `why` (no such look; a look with
  no icon).
- `WErr character_build(MpqChain *archives, const CharacterLook *look, Character *out, WWhy *why);`
- `void character_free(Character *c);`
