/*
 * 097-table-layouts.c (issues 106, 1109 step 1r) - see 097-table-layouts.h
 *
 * For a general audience: the layouts as data, one list of column runs a
 * table, and the few questions asked of them. A run's `read_by` names the
 * part of this client that reads those columns at that place (checked
 * from its code); NULL is unchecked (see the header). Field numbers in
 * the comments count from 1, as 016-dbc.h does.
 */
#include "097-table-layouts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define U TABLE_U32
#define I TABLE_I32
#define F TABLE_F32
#define S TABLE_STR
#define L TABLE_LOC
#define END { NULL, U, 0, NULL, 0, 0 }

/* {{{ the converter's tables (world-edit-to-execute's src/wowbuild/dbc.lua:
 * names, kinds and widths as its LAYOUTS have them) */
static const TableGroup MAP[] = {                       /* 66 */
    { "ID", U, 1, "074, 087", 0, 0 }, { "Directory", S, 1, "087", 0, 0 }, { "InstanceType", U, 1, NULL, 0, 0 },
    { "Flags", U, 1, NULL, 0, 0 }, { "PVP", U, 1, NULL, 0, 0 }, { "MapName", L, 1, NULL, 0, 1 },
    { "AreaTableID", U, 1, NULL, 0, 0 }, { "MapDescription0", L, 1, NULL, 0, 0 }, { "MapDescription1", L, 1, NULL, 0, 0 },
    { "LoadingScreenID", U, 1, NULL, 0, 0 }, { "MinimapIconScale", F, 1, NULL, 1, 0 }, { "CorpseMapID", I, 1, NULL, 0, 0 },
    { "CorpseX", F, 1, NULL, 0, 0 }, { "CorpseY", F, 1, NULL, 0, 0 }, { "TimeOfDayOverride", I, 1, NULL, 0, 0 },
    { "ExpansionID", U, 1, NULL, 0, 0 }, { "RaidOffset", U, 1, NULL, 0, 0 }, { "MaxPlayers", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup AREA_TABLE[] = {                /* 36 */
    { "ID", U, 1, "074", 0, 0 }, { "ContinentID", U, 1, NULL, 0, 0 }, { "ParentAreaID", U, 1, NULL, 0, 0 },
    { "AreaBit", U, 1, NULL, 0, 0 }, { "Flags", U, 1, NULL, 0, 0 }, { "SoundProviderPref", U, 1, NULL, 0, 0 },
    { "SoundProviderPrefUnderwater", U, 1, NULL, 0, 0 }, { "AmbienceID", U, 1, NULL, 0, 0 }, { "ZoneMusic", U, 1, NULL, 0, 0 },
    { "IntroSound", U, 1, NULL, 0, 0 }, { "ExplorationLevel", U, 1, NULL, 0, 0 }, { "AreaName", L, 1, NULL, 0, 1 },
    { "FactionGroupMask", U, 1, NULL, 0, 0 }, { "LiquidTypeID", U, 4, NULL, 0, 0 }, { "MinElevation", F, 1, NULL, 0, 0 },
    { "AmbientMultiplier", F, 1, NULL, 0, 0 }, { "LightID", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup LOADING_SCREENS[] = {           /* 4 */
    { "ID", U, 1, "074", 0, 0 }, { "Name", S, 1, NULL, 0, 1 }, { "FileName", S, 1, NULL, 0, 0 }, { "HasWideScreen", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup CREATURE_DISPLAY_INFO[] = {     /* 16 */
    { "ID", U, 1, "074, 088", 0, 0 }, { "ModelID", U, 1, "088", 0, 0 }, { "SoundID", U, 1, NULL, 0, 0 },
    { "ExtendedDisplayInfoID", U, 1, "041, 088", 0, 0 }, { "CreatureModelScale", F, 1, "041, 088", 1, 0 },
    { "CreatureModelAlpha", U, 1, NULL, 255, 0 }, { "TextureVariation", S, 3, "088", 0, 0 }, { "PortraitTextureName", S, 1, NULL, 0, 0 },
    { "SizeClass", U, 1, NULL, 0, 0 }, { "BloodID", U, 1, NULL, 0, 0 }, { "NPCSoundID", U, 1, NULL, 0, 0 },
    { "ParticleColorID", U, 1, NULL, 0, 0 }, { "CreatureGeosetData", U, 1, NULL, 0, 0 }, { "ObjectEffectPackageID", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup CREATURE_MODEL_DATA[] = {       /* 28 */
    { "ID", U, 1, "074, 088", 0, 0 }, { "Flags", U, 1, NULL, 0, 0 }, { "ModelName", S, 1, "088", 0, 0 },
    { "SizeClass", U, 1, NULL, 0, 0 }, { "ModelScale", F, 1, NULL, 1, 0 }, { "BloodID", U, 1, NULL, 0, 0 },
    { "FootprintTextureID", U, 1, NULL, 0, 0 }, { "FootprintTextureLength", F, 1, NULL, 0, 0 }, { "FootprintTextureWidth", F, 1, NULL, 0, 0 },
    { "FootprintParticleScale", F, 1, NULL, 1, 0 }, { "FoleyMaterialID", U, 1, NULL, 0, 0 }, { "FootstepShakeSize", U, 1, NULL, 0, 0 },
    { "DeathThudShakeSize", U, 1, NULL, 0, 0 }, { "SoundID", U, 1, NULL, 0, 0 }, { "CollisionWidth", F, 1, NULL, 0, 0 },
    { "CollisionHeight", F, 1, NULL, 0, 0 }, { "MountHeight", F, 1, NULL, 0, 0 }, { "GeoBoxMinX", F, 1, NULL, 0, 0 },
    { "GeoBoxMinY", F, 1, NULL, 0, 0 }, { "GeoBoxMinZ", F, 1, NULL, 0, 0 }, { "GeoBoxMaxX", F, 1, NULL, 0, 0 },
    { "GeoBoxMaxY", F, 1, NULL, 0, 0 }, { "GeoBoxMaxZ", F, 1, NULL, 0, 0 }, { "WorldEffectScale", F, 1, NULL, 1, 0 },
    { "AttachedEffectScale", F, 1, NULL, 1, 0 }, { "MissileCollisionRadius", F, 1, NULL, 0, 0 },
    { "MissileCollisionPush", F, 1, NULL, 0, 0 }, { "MissileCollisionRaise", F, 1, NULL, 0, 0 }, END,
};
static const TableGroup FACTION_TEMPLATE[] = {          /* 14 */
    { "ID", U, 1, "074", 0, 0 }, { "Faction", U, 1, NULL, 0, 0 }, { "Flags", U, 1, NULL, 0, 0 }, { "FactionGroup", U, 1, NULL, 0, 0 },
    { "FriendGroup", U, 1, NULL, 0, 0 }, { "EnemyGroup", U, 1, NULL, 0, 0 }, { "Enemies", U, 4, NULL, 0, 0 }, { "Friend", U, 4, NULL, 0, 0 }, END,
};
static const TableGroup FACTION[] = {                   /* 57 */
    { "ID", U, 1, "074", 0, 0 }, { "ReputationIndex", I, 1, NULL, 0, 0 }, { "ReputationRaceMask", U, 4, NULL, 0, 0 },
    { "ReputationClassMask", U, 4, NULL, 0, 0 }, { "ReputationBase", I, 4, NULL, 0, 0 }, { "ReputationFlags", U, 4, NULL, 0, 0 },
    { "ParentFactionID", U, 1, NULL, 0, 0 }, { "ParentFactionMod", F, 2, NULL, 0, 0 }, { "ParentFactionCap", U, 2, NULL, 0, 0 },
    { "Name", L, 1, NULL, 0, 1 }, { "Description", L, 1, NULL, 0, 0 }, END,
};
/* }}} */

/* {{{ her character's tables (025), and a humanoid creature's (041):
 * the columns 025 and 041 read are theirs; the rest recalled here */
static const TableGroup CREATURE_DISPLAY_INFO_EXTRA[] = {   /* 21: 041's own comment lists all 21 */
    { "ID", U, 1, "074, 041", 0, 0 }, { "DisplayRaceID", U, 1, "041", 0, 0 }, { "DisplaySexID", U, 1, "041", 0, 0 },
    { "SkinID", U, 1, "041", 0, 0 }, { "FaceID", U, 1, "041", 0, 0 }, { "HairStyleID", U, 1, "041", 0, 0 },
    { "HairColorID", U, 1, "041", 0, 0 }, { "FacialHairID", U, 1, "041", 0, 0 }, { "NPCItemDisplay", U, 11, "041", 0, 0 },
    { "Flags", U, 1, NULL, 0, 0 }, { "BakeName", S, 1, "041", 0, 0 }, END,
};
static const TableGroup CHAR_START_OUTFIT[] = {             /* 74 */
    { "ID", U, 1, "074", 0, 0 }, { "RaceClassSexOutfit", U, 1, "025", 0, 0 }, { "ItemID", I, 24, NULL, 0, 0 },
    { "DisplayItemID", I, 24, "025", 0, 0 }, { "InventoryType", I, 24, "025", 0, 0 }, END,
};
static const TableGroup ITEM_DISPLAY_INFO[] = {             /* 25 */
    { "ID", U, 1, "074, 025", 0, 0 }, { "ModelName", S, 2, "025", 0, 0 }, { "ModelTexture", S, 2, "025", 0, 0 },
    { "InventoryIcon1", S, 1, "025", 0, 0 }, { "InventoryIcon2", S, 1, NULL, 0, 0 }, { "GeosetGroup", U, 3, "025", 0, 0 },
    { "Flags", U, 1, NULL, 0, 0 }, { "SpellVisualID", U, 1, NULL, 0, 0 }, { "GroupSoundIndex", U, 1, NULL, 0, 0 },
    { "HelmetGeosetVis", U, 2, "025", 0, 0 }, { "Texture", S, 8, "025", 0, 0 }, { "ItemVisual", U, 1, NULL, 0, 0 },
    { "ParticleColorID", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup CHAR_HAIR_GEOSETS[] = {             /* 6 */
    { "ID", U, 1, "074", 0, 0 }, { "RaceID", U, 1, "025", 0, 0 }, { "SexID", U, 1, "025", 0, 0 },
    { "VariationID", U, 1, "025", 0, 0 }, { "GeosetID", U, 1, "025", 0, 0 }, { "Showscalp", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup CHARACTER_FACIAL_HAIR_STYLES[] = {  /* 8: no row number; its first column is the race */
    { "RaceID", U, 1, "025", 0, 0 }, { "SexID", U, 1, "025", 0, 0 }, { "VariationID", U, 1, "025", 0, 0 },
    { "Geoset", U, 5, "025", 0, 0 }, END,
};
static const TableGroup CHAR_SECTIONS[] = {                 /* 10 */
    { "ID", U, 1, "074", 0, 0 }, { "RaceID", U, 1, "025", 0, 0 }, { "SexID", U, 1, "025", 0, 0 },
    { "BaseSection", U, 1, "025", 0, 0 }, { "TextureName", S, 3, "025", 0, 0 }, { "Flags", U, 1, NULL, 0, 0 },
    { "VariationIndex", U, 1, "025", 0, 0 }, { "ColorIndex", U, 1, "025", 0, 0 }, END,
};
static const TableGroup HELMET_GEOSET_VIS_DATA[] = {        /* 8 */
    { "ID", U, 1, "074, 025", 0, 0 }, { "HideHair", U, 1, "025", 0, 0 }, { "HideFacialHair", U, 3, "025", 0, 0 },
    { "HideEars", U, 1, "025", 0, 0 }, { "HideEyebrows", U, 1, NULL, 0, 0 }, { "HidePaint", U, 1, NULL, 0, 0 }, END,
};
/* }}} */

/* {{{ the world's: water (040) and light (026) */
static const TableGroup LIQUID_TYPE[] = {                   /* 45 */
    { "ID", U, 1, "074, 040", 0, 0 }, { "Name", S, 1, NULL, 0, 1 }, { "Flags", U, 1, NULL, 0, 0 },
    { "Type", U, 1, "040", 0, 0 }, { "SoundID", U, 1, NULL, 0, 0 }, { "SpellID", U, 1, NULL, 0, 0 },
    { "MaxDarkenDepth", F, 1, NULL, 0, 0 }, { "FogDarkenIntensity", F, 1, NULL, 0, 0 }, { "AmbDarkenIntensity", F, 1, NULL, 0, 0 },
    { "DirDarkenIntensity", F, 1, NULL, 0, 0 }, { "LightID", U, 1, NULL, 0, 0 }, { "ParticleScale", F, 1, NULL, 1, 0 },
    { "ParticleMovement", U, 1, NULL, 0, 0 }, { "ParticleTexSlots", U, 1, NULL, 0, 0 }, { "MaterialID", U, 1, NULL, 0, 0 },
    { "Texture1", S, 1, "040", 0, 0 }, { "Texture2", S, 1, NULL, 0, 0 }, { "Texture3", S, 1, NULL, 0, 0 },
    { "Texture4", S, 1, NULL, 0, 0 }, { "Texture5", S, 1, NULL, 0, 0 }, { "Texture6", S, 1, NULL, 0, 0 }, { "Color", U, 2, NULL, 0, 0 },
    { "Float", F, 18, NULL, 0, 0 }, { "Int", U, 4, NULL, 0, 0 }, END,
};
static const TableGroup LIGHT[] = {                         /* 15 */
    { "ID", U, 1, "026", 0, 0 }, { "ContinentID", U, 1, "026", 0, 0 }, { "X", F, 1, "026", 0, 0 }, { "Y", F, 1, "026", 0, 0 },
    { "Z", F, 1, "026", 0, 0 }, { "FalloffStart", F, 1, "026", 0, 0 }, { "FalloffEnd", F, 1, "026", 0, 0 },
    { "LightParamsID1", U, 1, "026", 0, 0 }, { "LightParamsID2", U, 1, "026", 0, 0 }, { "LightParamsID3", U, 1, NULL, 0, 0 },
    { "LightParamsID4", U, 1, NULL, 0, 0 }, { "LightParamsID5", U, 1, NULL, 0, 0 }, { "LightParamsID6", U, 1, NULL, 0, 0 },
    { "LightParamsID7", U, 1, NULL, 0, 0 }, { "LightParamsID8", U, 1, NULL, 0, 0 }, END,
};
static const TableGroup LIGHT_INT_BAND[] = {                /* 34 */
    { "ID", U, 1, "026", 0, 0 }, { "Num", U, 1, "026", 0, 0 }, { "Time", U, 16, "026", 0, 0 }, { "Data", U, 16, "026", 0, 0 }, END,
};
static const TableGroup LIGHT_FLOAT_BAND[] = {              /* 34 */
    { "ID", U, 1, "026", 0, 0 }, { "Num", U, 1, "026", 0, 0 }, { "Time", U, 16, "026", 0, 0 }, { "Data", F, 16, "026", 0, 0 }, END,
};
static const TableGroup LIGHT_PARAMS[] = {                  /* 9 */
    { "ID", U, 1, "026", 0, 0 }, { "HighlightSky", U, 1, NULL, 0, 0 }, { "LightSkyboxID", U, 1, "026", 0, 0 },
    { "CloudTypeID", U, 1, NULL, 0, 0 }, { "Glow", F, 1, NULL, 0, 0 }, { "WaterShallowAlpha", F, 1, "026", 0.75, 0 },
    { "WaterDeepAlpha", F, 1, "026", 0.75, 0 }, { "OceanShallowAlpha", F, 1, "026", 0.75, 0 }, { "OceanDeepAlpha", F, 1, "026", 0.75, 0 }, END,
};
static const TableGroup LIGHT_SKYBOX[] = {                  /* 3 */
    { "ID", U, 1, "026", 0, 0 }, { "Name", S, 1, "026", 0, 0 }, { "Flags", U, 1, NULL, 0, 0 }, END,
};
/* }}} */

#undef U
#undef I
#undef F
#undef S
#undef L
#undef END

static const TableLayout LAYOUTS[] = {
    { "Map", 66, 1, "the converter's (world-edit-to-execute's dbc.lua)", MAP },
    { "AreaTable", 36, 1, "the converter's", AREA_TABLE },
    { "LoadingScreens", 4, 1, "the converter's", LOADING_SCREENS },
    { "CreatureDisplayInfo", 16, 1, "the converter's", CREATURE_DISPLAY_INFO },
    { "CreatureModelData", 28, 1, "the converter's", CREATURE_MODEL_DATA },
    { "FactionTemplate", 14, 1, "the converter's", FACTION_TEMPLATE },
    { "Faction", 57, 1, "the converter's", FACTION },
    { "CreatureDisplayInfoExtra", 21, 1, "041's own list of its columns", CREATURE_DISPLAY_INFO_EXTRA },
    { "CharStartOutfit", 74, 1, "025's last column read (74), the test program's made table", CHAR_START_OUTFIT },
    { "ItemDisplayInfo", 25, 1, "the test program's made table (025 reads to 23)", ITEM_DISPLAY_INFO },
    { "CharHairGeosets", 6, 1, "the test program's made table (025 reads to 5)", CHAR_HAIR_GEOSETS },
    { "CharacterFacialHairStyles", 8, 0, "025's last column read (8), the test program's made table", CHARACTER_FACIAL_HAIR_STYLES },
    { "CharSections", 10, 1, "025's last column read (10), the test program's made table", CHAR_SECTIONS },
    { "HelmetGeosetVisData", 8, 1, "recalled (025 reads to 6)", HELMET_GEOSET_VIS_DATA },
    { "LiquidType", 45, 1, "the baseline set's (085; 040 reads to 16)", LIQUID_TYPE },
    { "Light", 15, 1, "the baseline set's (085; 026 reads to 9)", LIGHT },
    { "LightIntBand", 34, 1, "the baseline set's (085), 026's check", LIGHT_INT_BAND },
    { "LightFloatBand", 34, 1, "the baseline set's (085), 026's check", LIGHT_FLOAT_BAND },
    { "LightParams", 9, 1, "the baseline set's (085; 026 reads to 9)", LIGHT_PARAMS },
    { "LightSkybox", 3, 1, "the baseline set's (085; 026 reads to 2)", LIGHT_SKYBOX },
};
#define NLAYOUTS ((int)(sizeof LAYOUTS / sizeof LAYOUTS[0]))

/* {{{ table_layout, table_layout_at */
const TableLayout *table_layout(const char *name)
{
    for (int i = 0; name && i < NLAYOUTS; ++i) if (!strcasecmp(LAYOUTS[i].name, name)) return &LAYOUTS[i];
    return NULL;
}
const TableLayout *table_layout_at(int i) { return i >= 0 && i < NLAYOUTS ? &LAYOUTS[i] : NULL; }
/* }}} */

/* {{{ table_layout_column, table_layout_read_to, table_layout_checked */
static uint32_t width_of(const TableGroup *g) { return (uint32_t)(g->count > 1 ? g->count : 1) * (g->type == TABLE_LOC ? TABLE_LOC_TEXTS + 1 : 1); }

int table_layout_column(const TableLayout *l, uint32_t k, TableColumn *out)
{
    memset(out, 0, sizeof *out);
    if (!l || k < 1 || k > l->fields) return 0;
    uint32_t at = 1;
    for (const TableGroup *g = l->groups; g->name; ++g) {
        uint32_t w = width_of(g);
        if (k < at + w) {
            uint32_t into = k - at, per = g->type == TABLE_LOC ? TABLE_LOC_TEXTS + 1 : 1, which = into / per, part = into % per;
            char base[48];
            if (g->count > 1) snprintf(base, sizeof base, "%s%u", g->name, which + 1);
            else snprintf(base, sizeof base, "%s", g->name);
            out->read_by = g->read_by;
            out->def = g->def;
            if (g->type != TABLE_LOC) { snprintf(out->name, sizeof out->name, "%s", base); out->type = g->type; out->unknown = g->unknown; }
            else if (part < TABLE_LOC_TEXTS) {
                static const char *const LANG[TABLE_LOC_TEXTS] = { "enUS", "koKR", "frFR", "deDE", "zhCN", "zhTW", "esES", "esMX",
                                                                    "ruRU", "jaJP", "ptPT", "itIT", "unk12", "unk13", "unk14", "unk15" };
                snprintf(out->name, sizeof out->name, "%s_%s", base, LANG[part]);
                out->type = TABLE_STR;
                out->unknown = g->unknown && part == 0;          /* the client's language, enUS, the first */
            } else {
                snprintf(out->name, sizeof out->name, "%s_mask", base);
                out->type = TABLE_U32;
                out->def = TABLE_LOC_MASK;
            }
            return 1;
        }
        at += w;
    }
    snprintf(out->name, sizeof out->name, "Column%u", k);   /* past its runs (a layout shorter than its width: none is) */
    out->type = TABLE_U32;
    return 1;
}

uint32_t table_layout_read_to(const TableLayout *l)
{
    uint32_t at = 1, last = 0;
    for (const TableGroup *g = l ? l->groups : NULL; g && g->name; ++g) {
        uint32_t w = width_of(g);
        if (g->read_by) last = at + w - 1;
        at += w;
    }
    return last;
}

uint32_t table_layout_checked(const TableLayout *l)
{
    uint32_t n = 0;
    for (const TableGroup *g = l ? l->groups : NULL; g && g->name; ++g) if (g->read_by) n += width_of(g);
    return n;
}
/* }}} */

/* {{{ table_layout_fits */
WErr table_layout_fits(const TableLayout *l, uint32_t fields, const char *where, WWhy *why)
{
    uint32_t to = table_layout_read_to(l);
    if (l && fields < to)
        return w_fail(why, W_ERR_CORRUPT, "%s.dbc from %s has %u fields a row, and this client reads its field %u: too narrow for 3.3.5a's (the layout says %u)",
                      l->name, where ? where : "a place", fields, to, l->fields);
    return W_OK;
}
/* }}} */

/* {{{ table_layout_placeholder */
static void put32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }

unsigned char *table_layout_placeholder(const TableLayout *l, uint32_t fields, uint32_t id, size_t *len)
{
    *len = 0;
    if (!l || !fields) return NULL;
    char unknown[96];
    snprintf(unknown, sizeof unknown, "unknown %s %u", l->name, id);
    size_t ulen = strlen(unknown) + 1;
    int any = 0;                                              /* is the visible name used (once, at offset 1) */
    TableColumn c;
    for (uint32_t k = 1; k <= fields && k <= l->fields; ++k) if (table_layout_column(l, k, &c) && c.type == TABLE_STR && c.unknown) any = 1;
    size_t text_len = 1 + (any ? ulen : 0);
    *len = 20 + (size_t)fields * 4 + text_len;
    unsigned char *b = calloc(1, *len);
    if (!b) { *len = 0; return NULL; }
    memcpy(b, "WDBC", 4);
    put32(b + 4, 1); put32(b + 8, fields); put32(b + 12, fields * 4); put32(b + 16, (uint32_t)text_len);
    unsigned char *row = b + 20;
    for (uint32_t k = 1; k <= fields && k <= l->fields; ++k) {
        table_layout_column(l, k, &c);
        uint32_t v = 0;
        switch (c.type) {
            case TABLE_F32: { float f = (float)c.def; memcpy(&v, &f, 4); break; }
            case TABLE_I32: v = (uint32_t)(int32_t)c.def; break;
            case TABLE_STR: v = c.unknown ? 1 : 0; break;
            default:        v = (uint32_t)c.def; break;
        }
        put32(row + (size_t)(k - 1) * 4, v);
    }
    if (l->has_id) put32(row, id);
    if (any) memcpy(row + (size_t)fields * 4 + 1, unknown, ulen);
    return b;
}
/* }}} */
