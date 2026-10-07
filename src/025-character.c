/*
 * 025-character.c (issue 173k) - see 025-character.h. Ported from
 * src/custom-client/lib/character.lua.
 */
#include "025-character.h"

#include "016-dbc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ races: ChrRaces ids and their model folder names */
/* the two letters item models carry for each race (helms are made per
 * race and sex: Helm_..._NiF.m2), by ChrRaces id */
static const char *race_prefix(int race)
{
    static const char *const P[12] = { NULL, "Hu", "Or", "Dw", "Ni", "Sc", "Ta", "Gn", "Tr", NULL, "Be", "Dr" };
    return (race >= 0 && race < 12) ? P[race] : NULL;
}
static const char *race_folder(int race)
{
    static const char *const NAMES[12] = { NULL, "Human", "Orc", "Dwarf", "NightElf", "Scourge", "Tauren", "Gnome", "Troll",
                                           NULL, "BloodElf", "Draenei" };
    return (race >= 0 && race < 12) ? NAMES[race] : NULL;
}
/* }}} */

/* {{{ the skin sheet's patches
 * Where each picture goes, as the 3.3.5a client has it built in, for a
 * 256 x 256 sheet (x, y, width, height); doubled for 512 x 512 sheets. */
enum { R_ARM_UPPER, R_ARM_LOWER, R_HAND, R_FACE_UPPER, R_FACE_LOWER, R_TORSO_UPPER, R_TORSO_LOWER, R_LEG_UPPER, R_LEG_LOWER, R_FOOT };
static const int REGION[10][4] = {
    { 0, 0, 128, 64 },            /* arm upper */
    { 0, 64, 128, 64 },           /* arm lower */
    { 0, 128, 128, 32 },          /* hand */
    { 0, 160, 128, 32 },          /* face upper */
    { 0, 192, 128, 64 },          /* face lower */
    { 128, 0, 128, 64 },          /* torso upper */
    { 128, 64, 128, 32 },         /* torso lower */
    { 128, 96, 128, 64 },         /* leg upper */
    { 128, 160, 128, 64 },        /* leg lower */
    { 128, 224, 128, 32 },        /* foot */
};
/* an item's picture for each region: ItemDisplayInfo's fields 16-23, in
 * the region's own folder (Item\TextureComponents\<folder>\<name>_<F, M or
 * U>.blp: the wearer's sex, or U for either) */
static const struct { int field, region; const char *folder; } ITEM_REGION[8] = {
    { 16, R_ARM_UPPER, "ArmUpperTexture" },   { 17, R_ARM_LOWER, "ArmLowerTexture" },
    { 18, R_HAND, "HandTexture" },            { 19, R_TORSO_UPPER, "TorsoUpperTexture" },
    { 20, R_TORSO_LOWER, "TorsoLowerTexture" }, { 21, R_LEG_UPPER, "LegUpperTexture" },
    { 22, R_LEG_LOWER, "LegLowerTexture" },   { 23, R_FOOT, "FootTexture" },
};
/* }}} */

/* {{{ static bare_pieces: which variant of each piece group a bare
 * character shows (173d; the owner, 2026-10-06: the game's own tables).
 * A group g shows piece g * 100 + variant; variant 0 shows nothing.
 *   groups 1, 3, 2 and 17: CharacterFacialHairStyles.dbc's row for the
 *     race, sex and facial-hair choice. Its columns (3.3.5a, no id): 1
 *     race, 2 sex, 3 the choice, 4-8 five variants: group 1, group 3,
 *     group 2, group 16 (always 0), group 17 (the race's eye glow: a night
 *     elf's 2, so 1702). The order 1, 3, 2 was measured: across every
 *     playable race and sex, 185 of the variants it names are pieces the
 *     model has, against 127 for 1, 2, 3; the 27 it misses miss either way
 *     (a variant 1 on a model with no x01 piece: bare).
 *   group 0 below 100: the hair style (CharHairGeosets, the caller's).
 *   group 7, the ears: variant 2 (701 is earless; every race's ears are
 *     702). No table names it; the client's own rule.
 *   every other group: variant 1, its bare shape (401 bare hands, 501 bare
 *     feet, 1301 bare legs, 1501 the back without a cape ...), where the
 *     model has it; items change these (wear).
 * A choice with no row is an error, said with the race and sex, for a
 * player's look. A creature's look (look->npc) may use choices players
 * can't pick (an Astranaar sentinel's 10, where a night-elf woman's rows
 * stop at 9); it takes the race's choice-0 row instead (keeping the eye
 * glow), said once per race, sex and choice. A FALLBACK: what the
 * original shows there is unknown (173t's open questions; check list). */
static int bare_pieces(MpqChain *archives, const CharacterLook *look, int variant[20], char *why, int why_len)
{
    for (int g = 0; g < 20; ++g) variant[g] = 1;
    variant[7] = 2;
    size_t len;
    unsigned char *t = mpq_chain_read(archives, "DBFilesClient\\CharacterFacialHairStyles.dbc", &len, NULL);
    Dbc d;
    char w2[300];
    if (!t || dbc_open(&d, t, len, w2, sizeof w2)) { free(t); snprintf(why, why_len, "CharacterFacialHairStyles: %s", t ? w2 : "not in the archives"); return -1; }
    int found = 0;
    int choice = look->facial_hair;
    for (int pass = 0; pass < 2 && !found; ++pass) {
        for (uint32_t r = 0; r < d.rows && !found; ++r) {
            if ((int)dbc_u32(&d, r, 1) != look->race || (int)dbc_u32(&d, r, 2) != look->sex || (int)dbc_u32(&d, r, 3) != choice) continue;
            static const int GROUP[5] = { 1, 3, 2, 16, 17 };
            for (int i = 0; i < 5; ++i) variant[GROUP[i]] = (int)dbc_u32(&d, r, 4 + (uint32_t)i);
            found = 1;
        }
        if (found || !look->npc || choice == 0) break;
        /* a creature's choice beyond the table: the race's choice 0 (said once) */
        static int said[64];
        static int nsaid;
        int key = look->race * 4096 + look->sex * 2048 + look->facial_hair, seen = 0;
        for (int i = 0; i < nsaid; ++i) seen |= said[i] == key;
        if (!seen && nsaid < 64) {
            said[nsaid++] = key;
            fprintf(stderr, "character: FALLBACK: a creature's facial-hair choice %d for race %d sex %d isn't in CharacterFacialHairStyles; it takes choice 0's pieces (173t)\n",
                    look->facial_hair, look->race, look->sex);
        }
        choice = 0;
    }
    free(t);
    if (!found) { snprintf(why, why_len, "no facial-hair choice %d for race %d sex %d in CharacterFacialHairStyles", look->facial_hair, look->race, look->sex); return -1; }
    return 0;
}
/* }}} */

/* {{{ static paste
 * Lays `img` onto `sheet` (RGBA, sw wide) inside region r scaled by k,
 * stretched by nearest pixel, blended by the picture's own alpha. */
static void paste(uint8_t *sheet, int sw, const BlpImage *img, const int *r, int k)
{
    int rx = r[0] * k, ry = r[1] * k, rw = r[2] * k, rh = r[3] * k;
    for (int y = 0; y < rh; ++y) {
        int sy = y * img->height / rh;
        for (int x = 0; x < rw; ++x) {
            int sx = x * img->width / rw;
            const uint8_t *s = img->rgba + ((size_t)sy * img->width + sx) * 4;
            double a = s[3] / 255.0;
            if (a > 0) {
                uint8_t *o = sheet + ((size_t)(ry + y) * sw + rx + x) * 4;
                for (int c = 0; c < 3; ++c) o[c] = (uint8_t)(o[c] * (1 - a) + s[c] * a + 0.5);
                o[3] = 255;
            }
        }
    }
}
/* }}} */

/* {{{ static picture
 * -1 and `why` on a fault; -2 when the archives hold no such file (the
 * layers above the skin treat that as "not painted", see below). */
static int picture(MpqChain *c, const char *name, BlpImage *img, char *why, int why_len)
{
    size_t len;
    unsigned char *s = mpq_chain_read(c, name, &len, NULL);
    if (!s) { snprintf(why, why_len, "the client's archives hold no %s", name); return -2; }
    char w2[200];
    int rc = blp_decode(s, len, img, w2, sizeof w2);
    free(s);
    if (rc) snprintf(why, why_len, "%s: %s", name, w2);
    return rc;
}
/* }}} */

/* {{{ static wear: each slot's item painted onto the skin sheet, and the
 * body pieces it asks for switched on (173p)
 * ItemDisplayInfo (3.3.5a): 1 id, 2-3 model names, 4-5 model pictures,
 * 8-10 its piece choices (gg[0..2]), 16-23 its region pictures.
 * The pieces, this client's reading of the original's rule (to check
 * against it): a group g shows piece g*100 + variant, variant 1 when
 * bare. Shirt and chest: sleeves g8 = 1 + gg0, g10 = 1 + gg1, and a robe
 * (gg2) its skirt g13 = 1 + gg2 (chest only). Legs: g9 = 1 + gg0, g11 =
 * 1 + gg1 when gg1, g13 = 1 + gg2 when gg2. Feet g5, hands g4, waist g18,
 * back (the cape) g15 = 1 + gg0; the tabard g12 = 2. Slots are applied in
 * their painting order, so a robe's skirt wins over the legs'. A group
 * with no piece of the chosen variant shows nothing (said once). */
static int wear(MpqChain *archives, const CharacterLook *look, Character *out, int k, char *why, int why_len)
{
    int any = 0;
    for (int s = 0; s < SLOT_COUNT; ++s) any |= look->items[s] != 0;
    if (!any) return 0;
    size_t len;
    unsigned char *t = mpq_chain_read(archives, "DBFilesClient\\ItemDisplayInfo.dbc", &len, NULL);
    Dbc d;
    char w2[300];
    if (!t || dbc_open(&d, t, len, w2, sizeof w2)) { free(t); snprintf(why, why_len, "ItemDisplayInfo: %s", t ? w2 : "not in the archives"); return -1; }
    int variant[20];
    for (int g = 0; g < 20; ++g) variant[g] = -1;
    for (int s = 0; s < SLOT_COUNT; ++s) {
        if (!look->items[s]) continue;
        int row = -1;
        for (uint32_t r = 0; r < d.rows; ++r) if ((int)dbc_u32(&d, r, 1) == look->items[s]) { row = (int)r; break; }
        if (row < 0) { fprintf(stderr, "character: no item look %d in ItemDisplayInfo; slot %d is left bare\n", look->items[s], s); continue; }
        int gg[3] = { (int)dbc_u32(&d, (uint32_t)row, 8), (int)dbc_u32(&d, (uint32_t)row, 9), (int)dbc_u32(&d, (uint32_t)row, 10) };
        switch (s) {
            case SLOT_SHIRT: case SLOT_CHEST:
                variant[8] = 1 + gg[0]; variant[10] = 1 + gg[1];
                if (s == SLOT_CHEST && gg[2]) variant[13] = 1 + gg[2];
                break;
            case SLOT_LEGS:
                variant[9] = 1 + gg[0];
                if (gg[1]) variant[11] = 1 + gg[1];
                if (gg[2]) variant[13] = 1 + gg[2];
                break;
            case SLOT_FEET:   variant[5] = 1 + gg[0]; break;
            case SLOT_HANDS:  variant[4] = 1 + gg[0]; break;
            case SLOT_WAIST:  variant[18] = 1 + gg[0]; break;
            case SLOT_BACK:   variant[15] = 1 + gg[0]; break;
            case SLOT_TABARD: variant[12] = 2; break;
            default: break;
        }
        /* the region pictures, the wearer's sex first, then either sex */
        for (int r = 0; r < 8; ++r) {
            const char *name = dbc_text(&d, (uint32_t)row, (uint32_t)ITEM_REGION[r].field);
            if (!name || !name[0]) continue;
            char path[300];
            BlpImage img;
            snprintf(path, sizeof path, "Item\\TextureComponents\\%s\\%s_%s.blp", ITEM_REGION[r].folder, name, look->sex == 1 ? "F" : "M");
            int got = picture(archives, path, &img, w2, sizeof w2);
            if (got == -2) {
                snprintf(path, sizeof path, "Item\\TextureComponents\\%s\\%s_U.blp", ITEM_REGION[r].folder, name);
                got = picture(archives, path, &img, w2, sizeof w2);
            }
            if (got == -2) { fprintf(stderr, "character: item look %d's %s picture %s is in no archive; left unpainted\n", look->items[s], ITEM_REGION[r].folder, name); continue; }
            if (got) { free(t); snprintf(why, why_len, "%s", w2); return -1; }
            paste(out->skin.rgba, out->skin.width, &img, REGION[ITEM_REGION[r].region], k);
            blp_free(&img);
        }
    }
    /* attached models (helm, shoulders, weapons), the helm's hidden
     * pieces, and the cloak's picture */
    for (int s = SLOT_HEAD; s < SLOT_COUNT; ++s) {
        if (!look->items[s]) continue;
        int row = -1;
        for (uint32_t r = 0; r < d.rows; ++r) if ((int)dbc_u32(&d, r, 1) == look->items[s]) { row = (int)r; break; }
        if (row < 0) continue;                                /* said above */
        char base[260];
        const char *m0 = dbc_text(&d, (uint32_t)row, 2), *m1 = dbc_text(&d, (uint32_t)row, 3);
        const char *p0 = dbc_text(&d, (uint32_t)row, 4), *p1 = dbc_text(&d, (uint32_t)row, 5);
        #define STRIP(dst, src) do { snprintf(dst, sizeof dst, "%s", src); char *dot_ = strrchr(dst, '.'); if (dot_) *dot_ = '\0'; } while (0)
        #define HANG(folder, name, pic, pt) do { if ((name)[0] && out->nattached < 6) { \
                CharacterAttachment *a_ = &out->attached[out->nattached++]; \
                STRIP(base, name); \
                snprintf(a_->model, sizeof a_->model, "Item\\ObjectComponents\\%s\\%.200s.m2", folder, base); \
                snprintf(a_->picture, sizeof a_->picture, "Item\\ObjectComponents\\%s\\%.200s.blp", folder, pic); \
                a_->point = pt; } } while (0)
        if (s == SLOT_HEAD && m0[0]) {
            const char *pre = race_prefix(look->race);
            if (pre && out->nattached < 6) {
                CharacterAttachment *a = &out->attached[out->nattached++];
                STRIP(base, m0);
                snprintf(a->model, sizeof a->model, "Item\\ObjectComponents\\Head\\%.200s_%s%s.m2", base, pre, look->sex == 1 ? "F" : "M");
                snprintf(a->picture, sizeof a->picture, "Item\\ObjectComponents\\Head\\%.200s.blp", p0);
                a->point = 11;
            }
            /* what the helm hides (HelmetGeosetVisData: 2 hair, 3-5 facial
             * hair groups 1-3, 6 ears; each a mask of races, bit race - 1),
             * the row named by the look's field 14 (men) or 15 (women) */
            uint32_t vis = dbc_u32(&d, (uint32_t)row, look->sex == 1 ? 15 : 14);
            size_t vlen;
            unsigned char *vt = vis ? mpq_chain_read(archives, "DBFilesClient\\HelmetGeosetVisData.dbc", &vlen, NULL) : NULL;
            Dbc vd;
            if (vt && !dbc_open(&vd, vt, vlen, w2, sizeof w2))
                for (uint32_t r = 0; r < vd.rows; ++r) {
                    if (dbc_u32(&vd, r, 1) != vis) continue;
                    uint32_t bit = 1u << (look->race - 1);
                    static const int GROUP[5] = { 0, 1, 2, 3, 7 };
                    for (int k = 0; k < 5; ++k) {
                        if (!(dbc_u32(&vd, r, 2 + (uint32_t)k) & bit)) continue;
                        for (uint32_t i = 0; i < out->model.npieces; ++i) {
                            uint32_t id = out->model.pieces[i].id;
                            if ((GROUP[k] == 0 && id > 0 && id < 100) || (GROUP[k] > 0 && id / 100 == (uint32_t)GROUP[k])) out->shown[i] = 0;
                        }
                    }
                }
            free(vt);
        }
        if (s == SLOT_SHOULDERS) { HANG("Shoulder", m0, p0, 6); HANG("Shoulder", m1, p1, 5); }
        if (s == SLOT_MAIN_HAND) HANG("Weapon", m0, p0, 1);
        if (s == SLOT_OFF_HAND) {
            if (!strncasecmp(m0, "Shield", 6) || !strncasecmp(m0, "Buckler", 7)) HANG("Shield", m0, p0, 0);
            else HANG("Weapon", m0, p0, 2);
        }
        if (s == SLOT_BACK && p0[0]) {
            char path[300];
            snprintf(path, sizeof path, "Item\\ObjectComponents\\Cape\\%.200s.blp", p0);
            int got = picture(archives, path, &out->cape, w2, sizeof w2);
            if (got == -2) fprintf(stderr, "character: the cloak's picture %s is in no archive\n", path);
            else if (got) { free(t); snprintf(why, why_len, "%s", w2); return -1; }
        }
        #undef HANG
        #undef STRIP
    }
    free(t);
    /* the pieces: each chosen group shows its variant alone */
    for (int g = 1; g < 20; ++g) {
        if (variant[g] < 0) continue;
        uint32_t want = (uint32_t)(g * 100 + variant[g]);
        int found = 0;
        for (uint32_t i = 0; i < out->model.npieces; ++i) {
            uint32_t id = out->model.pieces[i].id;
            if (id / 100 != (uint32_t)g) continue;
            out->shown[i] = id == want;
            found |= id == want;
        }
        /* variant 1 missing is a group with nothing bare (sleeves, belts):
         * no fault; a missing higher variant is the item asking for a
         * shape the model lacks, said once */
        if (!found && variant[g] > 1) fprintf(stderr, "character: the model has no piece %u (group %d's choice); the group shows nothing\n", want, g);
    }
    return 0;
}
/* }}} */

/* {{{ character_start_outfit
 * CharStartOutfit.dbc (3.3.5a, 296 bytes a row): 0 id; 4-7 race, class,
 * sex, outfit (a byte each); 8 24 item ids; 104 24 item looks
 * (ItemDisplayInfo ids); 200 24 inventory types; unused entries -1 or 0.
 * The inventory type says the slot: 1 head, 3 shoulders, 4 shirt, 5
 * chest and 20 robe, 6 waist, 7 legs, 8 feet, 9 wrists, 10 hands, 16
 * back, 19 tabard; 13 one-hand, 17 two-hand and 21 main hand to the main
 * hand; 14 shield, 22 off hand and 23 held to the off hand. The rest (15
 * bow, 25 thrown, 26 gun, 24 ammunition, 18 bag, 0 food and stones) have
 * no slot here. */
int character_start_outfit(MpqChain *archives, int race, int class_id, int sex, int items[SLOT_COUNT], char *why, int why_len)
{
    static const int SLOT_OF[27] = {
        [1] = SLOT_HEAD, [3] = SLOT_SHOULDERS, [4] = SLOT_SHIRT, [5] = SLOT_CHEST, [20] = SLOT_CHEST, [6] = SLOT_WAIST,
        [7] = SLOT_LEGS, [8] = SLOT_FEET, [9] = SLOT_WRISTS, [10] = SLOT_HANDS, [16] = SLOT_BACK, [19] = SLOT_TABARD,
        [13] = SLOT_MAIN_HAND, [17] = SLOT_MAIN_HAND, [21] = SLOT_MAIN_HAND, [14] = SLOT_OFF_HAND, [22] = SLOT_OFF_HAND, [23] = SLOT_OFF_HAND,
    };
    static const int HAS_SLOT[27] = { [1] = 1, [3] = 1, [4] = 1, [5] = 1, [20] = 1, [6] = 1, [7] = 1, [8] = 1, [9] = 1, [10] = 1, [16] = 1, [19] = 1,
                                      [13] = 1, [17] = 1, [21] = 1, [14] = 1, [22] = 1, [23] = 1 };
    size_t len;
    unsigned char *t = mpq_chain_read(archives, "DBFilesClient\\CharStartOutfit.dbc", &len, NULL);
    Dbc d;
    char w2[300];
    if (!t || dbc_open(&d, t, len, w2, sizeof w2)) { free(t); snprintf(why, why_len, "CharStartOutfit: %s", t ? w2 : "not in the archives"); return -1; }
    int found = 0;
    for (uint32_t r = 0; r < d.rows && !found; ++r) {
        uint32_t packed = dbc_u32(&d, r, 2);              /* race, class, sex, outfit: a byte each, lowest first */
        if ((int)(packed & 0xff) != race || (int)(packed >> 8 & 0xff) != class_id || (int)(packed >> 16 & 0xff) != sex) continue;
        found = 1;
        for (uint32_t k = 0; k < 24; ++k) {
            uint32_t look = dbc_u32(&d, r, 27 + k), kind = dbc_u32(&d, r, 51 + k);
            if (look == 0 || look == 0xffffffffu || kind >= 27 || !HAS_SLOT[kind]) continue;
            items[SLOT_OF[kind]] = (int)look;
        }
    }
    free(t);
    if (!found) { snprintf(why, why_len, "no starting outfit for race %d class %d sex %d in CharStartOutfit", race, class_id, sex); return -1; }
    return 0;
}
/* }}} */

/* {{{ character_build */
int character_build(MpqChain *archives, const CharacterLook *look, Character *out, char *why, int why_len)
{
    memset(out, 0, sizeof *out);
    const char *folder = race_folder(look->race);
    if (!folder) { snprintf(why, why_len, "no model folder known for race %d", look->race); return -1; }
    const char *sexname = look->sex == 1 ? "Female" : "Male";
    char base[200], name[260];
    snprintf(base, sizeof base, "Character\\%s\\%s\\%s%s", folder, sexname, folder, sexname);
    snprintf(out->base, sizeof out->base, "%s", base);

    size_t m2_len, skin_len;
    snprintf(name, sizeof name, "%s.m2", base);
    unsigned char *m2 = mpq_chain_read(archives, name, &m2_len, NULL);
    snprintf(name, sizeof name, "%s00.skin", base);
    unsigned char *skin = mpq_chain_read(archives, name, &skin_len, NULL);
    if (!m2 || !skin) { free(m2); free(skin); snprintf(why, why_len, "the client's archives hold no %s model or skin", base); return -1; }
    char w2[300];
    int rc = m2_read(m2, m2_len, skin, skin_len, &out->model, w2, sizeof w2);
    free(m2); free(skin);
    if (rc) { snprintf(why, why_len, "%s: %s", base, w2); return -1; }

    /* the hair style's piece (CharHairGeosets: ID, race, sex, variation, geoset, ...) */
    size_t len;
    unsigned char *t = mpq_chain_read(archives, "DBFilesClient\\CharHairGeosets.dbc", &len, NULL);
    Dbc d;
    if (dbc_open(&d, t, len, w2, sizeof w2)) { free(t); snprintf(why, why_len, "CharHairGeosets: %s", w2); return -1; }
    long hair_piece = -1;
    for (uint32_t r = 0; r < d.rows; ++r)
        if ((int)dbc_u32(&d, r, 2) == look->race && (int)dbc_u32(&d, r, 3) == look->sex && (int)dbc_u32(&d, r, 4) == look->hair_style)
            hair_piece = dbc_u32(&d, r, 5);
    free(t);
    if (hair_piece < 0) { snprintf(why, why_len, "no hair style %d for race %d sex %d", look->hair_style, look->race, look->sex); return -1; }

    int variant[20];
    if (bare_pieces(archives, look, variant, why, why_len)) return -1;
    out->shown = calloc(out->model.npieces ? out->model.npieces : 1, 1);
    for (uint32_t i = 0; i < out->model.npieces; ++i) {
        uint32_t id = out->model.pieces[i].id, g = id / 100;
        out->shown[i] = id == 0 || id == (uint32_t)hair_piece || (g > 0 && g < 20 && variant[g] > 0 && id == g * 100 + (uint32_t)variant[g]);
    }

    /* the pictures: CharSections rows are ID, race, sex, section (0 skin,
     * 1 face, 2 facial hair, 3 hair, 4 underwear), three texture names,
     * flags, variation, colour */
    t = mpq_chain_read(archives, "DBFilesClient\\CharSections.dbc", &len, NULL);
    if (dbc_open(&d, t, len, w2, sizeof w2)) { free(t); snprintf(why, why_len, "CharSections: %s", w2); return -1; }
    char sect[5][3][260];
    int have[5] = { 0 };
    for (uint32_t r = 0; r < d.rows; ++r) {
        if ((int)dbc_u32(&d, r, 2) != look->race || (int)dbc_u32(&d, r, 3) != look->sex) continue;
        int s = (int)dbc_u32(&d, r, 4), variation = (int)dbc_u32(&d, r, 9), colour = (int)dbc_u32(&d, r, 10);
        int want_v, want_c;
        switch (s) {                          /* which variation and colour each section wants */
            case 0: want_v = 0;                want_c = look->skin;       break;
            case 1: want_v = look->face;       want_c = look->skin;       break;
            case 3: want_v = look->hair_style; want_c = look->hair_color; break;
            case 4: want_v = 0;                want_c = look->skin;       break;
            default: continue;
        }
        if (variation == want_v && colour == want_c) {
            for (int k = 0; k < 3; ++k) snprintf(sect[s][k], 260, "%s", dbc_text(&d, r, 5 + k));
            have[s] = 1;
        }
    }
    free(t);
    if (!have[0]) { snprintf(why, why_len, "no skin colour %d", look->skin); return -1; }

    /* the skin sheet: body, then face, scalp, underwear on top. The body
     * must exist. A layer above it that CharSections names but the
     * archives lack is left unpainted, as the original leaves it (found
     * with a tauren man: CharSections names Character\Tauren\
     * ScalpUpperHair00_00.blp, which no archive holds); each such layer
     * is reported by name. */
    if (picture(archives, sect[0][0], &out->skin, why, why_len)) return -1;
    int k = out->skin.width / 256;
    struct { int s, t, region; } lays[] = {
        { 1, 0, R_FACE_LOWER }, { 1, 1, R_FACE_UPPER },
        { 3, 1, R_FACE_LOWER }, { 3, 2, R_FACE_UPPER },
        { 4, 0, R_LEG_UPPER },  { 4, 1, R_TORSO_UPPER },
    };
    for (size_t i = 0; i < sizeof lays / sizeof lays[0]; ++i) {
        if (!have[lays[i].s] || !sect[lays[i].s][lays[i].t][0]) continue;
        BlpImage img;
        int got = picture(archives, sect[lays[i].s][lays[i].t], &img, why, why_len);
        if (got == -2) { fprintf(stderr, "character: %s; that layer is left unpainted\n", why); continue; }
        if (got) return -1;
        paste(out->skin.rgba, out->skin.width, &img, REGION[lays[i].region], k);
        blp_free(&img);
    }
    /* the fur (kind 8): the skin row's second picture, where the race has
     * one (a tauren's tail and coat; found 2026-10-06, the owner's missing
     * tail: the pieces using it were skipped for want of a picture) */
    if (sect[0][1][0]) {
        int got = picture(archives, sect[0][1], &out->fur, why, why_len);
        if (got == -2) fprintf(stderr, "character: %s; the fur is left unpainted\n", why);
        else if (got) return -1;
    }
    if (have[3] && sect[3][0][0]) {
        int got = picture(archives, sect[3][0], &out->hair, why, why_len);
        if (got == -2) fprintf(stderr, "character: %s; the hair is left unpainted\n", why);
        else if (got) return -1;
    }
    return wear(archives, look, out, k, why, why_len);
}
/* }}} */

/* {{{ character_free */
void character_free(Character *c)
{
    m2_free(&c->model);
    free(c->shown);
    blp_free(&c->skin);
    blp_free(&c->hair);
    blp_free(&c->fur);
    blp_free(&c->cape);
}
/* }}} */
