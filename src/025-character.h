/*
 * 025-character.h (issue 173k; the C twin of lib/character.lua) - one player
 * character, put together
 *
 * For a general audience: a race's model holds every hair style and
 * clothing shape at once, and its skin is painted from several pictures
 * laid onto one sheet (body, face, scalp, underwear). The game picks which
 * pieces show and which pictures go where from its own tables
 * (CharHairGeosets, CharacterFacialHairStyles, CharSections); this does
 * the same for one character's choices, and dresses it in its items
 * (173p).
 */
#ifndef EG_CHARACTER_H
#define EG_CHARACTER_H

#include "013-errors.h"

#include "017-blp.h"
#include "022-m2.h"
#include "015-mpq.h"
#include "060-resolver.h"

/* Where every file below comes from: through `assets` when a resolver is
 * open (a test-map run: packs, the server's folders, the map's own
 * folder, the original archives, then the baseline set; never a
 * placeholder), else the archives alone (`archives`), as before (1109
 * step 1m). A table is read whole, the first place's copy. Either may be
 * NULL, not both, and whichever is given must outlive the call. */

/* the slots a character wears things in (173p), in the order they are
 * painted onto the skin sheet when two cover the same region: what lies
 * under is painted first */
enum {
    SLOT_SHIRT, SLOT_LEGS, SLOT_FEET, SLOT_WRISTS, SLOT_CHEST, SLOT_HANDS, SLOT_TABARD, SLOT_WAIST,
    SLOT_HEAD, SLOT_SHOULDERS, SLOT_BACK, SLOT_MAIN_HAND, SLOT_OFF_HAND, SLOT_COUNT
};

typedef struct {
    int race, sex;                         /* ChrRaces id; 0 male, 1 female */
    int skin, face, hair_style, hair_color;/* the character-creation choices, from 0 */
    int items[SLOT_COUNT];                 /* each slot's item look (ItemDisplayInfo.dbc id), 0 for none (173p) */
    int facial_hair;                       /* the creation choice of facial hair (or a woman's earrings, markings ...), from 0
                                            * (last, so the looks written out by position keep their meaning) */
    int npc;                               /* 1: a creature's look (CreatureDisplayInfoExtra, 173t), whose choices may lie
                                            * outside the ones players can pick (see 025-character.c's bare_pieces) */
} CharacterLook;

/* an item model hung from one of the character's attachment points (173p):
 * model and picture archive paths, and the point (M2 attachment id: 1
 * right hand, 2 left hand, 0 shield, 5 right shoulder, 6 left shoulder,
 * 11 helm) */
typedef struct { char model[260]; char picture[260]; int point; } CharacterAttachment;

typedef struct {
    M2Model  model;                        /* the whole model */
    uint8_t *shown;                        /* per piece: 1 when it shows */
    BlpImage skin;                         /* texture kind 1: the finished skin sheet */
    BlpImage hair;                         /* texture kind 6 (width 0 when none) */
    BlpImage fur;                          /* texture kind 8: the skin row's second picture, a tauren's fur
                                            * and tail (width 0 when the race has none) */
    char     base[200];                    /* the model's archive path without its extension (for its .anim files) */
    CharacterAttachment attached[6];       /* helm, shoulders, weapons (173p) */
    int      nattached;
    BlpImage cape;                         /* texture kind 2: the cloak's picture (width 0 when none) */
    int      items_unpainted;              /* item pictures the archives lack, left off the skin sheet (each also
                                            * named on the terminal); 0 when every worn item is whole. The client
                                            * draws on; the dressing room (410) refuses the picture, a stand-in */
} Character;

/* A fresh character's clothes and weapons: the client's starting-outfit
 * table (CharStartOutfit.dbc) row for race, class (ChrClasses id: 1
 * warrior, 3 hunter, 11 druid ...) and sex, its items' looks put into
 * `items` by the kind of slot each fills. Slots it doesn't fill are left
 * as they were. Ranged weapons (bows, guns, thrown), ammunition, bags and
 * food have no slot here and are left out. W_OK, or a code with `why` (no such
 * row). */
WErr  character_start_outfit(Resolver *assets, MpqChain *archives, int race, int class_id, int sex, int items[SLOT_COUNT], WWhy *why);

/* Items to wear, written as text (410; the client's --wear): "slot:look"
 * pairs joined by commas, e.g. "chest:41143,hands:41138", each look (an
 * ItemDisplayInfo id) put into `items` at its slot. Slot names: shirt
 * legs feet wrists chest hands tabard waist head shoulders back mainhand
 * offhand. 0, or 1 with the part it couldn't read copied into `bad`. */
int   character_wear_text(int items[SLOT_COUNT], const char *text, char *bad, int bad_len);

/* An item look's icon (410): the archive path of the picture the game
 * shows for the item in bags and on the character pane
 * (Interface\Icons\<name>.blp, from ItemDisplayInfo), written into
 * `path`. W_OK, or W_ERR_NOT_FOUND with `why` (no such look; a look with
 * no icon). */
WErr  character_item_icon(Resolver *assets, MpqChain *archives, int look, char *path, size_t path_len, WWhy *why);

/* The character `look` describes, into `out` (freed with character_free).
 * W_OK, or a code with `why`; on a failure nothing is left to free. */
WErr  character_build(Resolver *assets, MpqChain *archives, const CharacterLook *look, Character *out, WWhy *why);
void character_free(Character *c);

#endif
