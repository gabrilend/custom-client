/*
 * 088-figure-model.h (issues 173t, 1109) - which files a figure is made from
 *
 * For a general audience: before a creature (or, in a test map with no
 * install, the player) can be drawn, the client must find its model.
 * This finds it, with no window, the way 041-figure.c then builds it: a
 * creature's display row (CreatureDisplayInfo, through the tables: a
 * converted map's own rows first) names its size, its skins and, through
 * CreatureModelData, its model; a humanoid row names a character look
 * instead (built by 025, which still needs the game's archives). A model
 * named .mdx that a converted map brought is read as named (Warcraft
 * III's MDLX); any other is the game's .m2 and its first view, from the
 * first place that has them (060: packs, the map's folder, the
 * installs, the baseline set). When no place has them and a resolver is
 * open, the baseline set's stand-in figure answers (085: a column a
 * person's height with one still bone and a Stand animation, its face
 * along the way it faces), said, so the creature still stands where it
 * should; it answers too where only the set answers the model (the set
 * stands its skeleton-less post in for every model the converter named,
 * and a creature needs a skeleton to stand and walk with). With no
 * resolver, or no set, the model is not found, as before.
 *
 * Window-free (CORE: the test program and the executable); 041 builds
 * what it finds.
 */
#ifndef EG_FIGURE_MODEL_H
#define EG_FIGURE_MODEL_H

#include "013-errors.h"
#include "015-mpq.h"
#include "060-resolver.h"
#include "074-tables.h"

typedef enum {
    FIGURE_MODEL_GAME,          /* the game's .m2 and its .skin */
    FIGURE_MODEL_WC3,           /* a WC3 model a converted map brought (MDLX: 061 reads it) */
    FIGURE_MODEL_HUMANOID,      /* a character look (CreatureDisplayInfoExtra): 025 builds it */
    FIGURE_MODEL_STAND_IN       /* the baseline set's stand-in figure, where no place has the model */
} FigureModelKind;

/* What was found (owned until figure_model_free):
 *   kind, display  what it is, and the display row asked for (0 for the player's stand-in)
 *   scale          the display's size (1 when the row gives none sensible)
 *   extra          HUMANOID: its CreatureDisplayInfoExtra row
 *   named          the model CreatureModelData names ("" for the player's stand-in)
 *   base           the model read, without its extension (GAME, STAND_IN):
 *                  its skeleton is in base.m2, its .anim files beside it
 *   folder         the named model's folder, with its last backslash (its skins)
 *   skins          the display's three skin names (pictures of kind 11 to 13)
 *   model, skin    the model's bytes (MDLX for WC3) and, but for WC3 and
 *                  HUMANOID, its view (.skin), with where each came from
 *   stood_in_for   STAND_IN: what no place had, said
 *   placeholder    1 when its display or model row was a placeholder row
 *                  (074; 1109 step 1q): the stand-in stands in for it */
typedef struct {
    FigureModelKind kind;
    int         display;
    float       scale;
    uint32_t    extra;
    char        named[260], base[260], folder[260];
    char        skins[3][128];
    AssetAnswer model, skin;
    char        stood_in_for[320];
    int         placeholder;
} FigureModel;

/* A creature display's model. W_OK with `*out`. A display row or its
 * model's row that no place has: with a resolver, a placeholder row
 * stands in (074's tables_row_or_placeholder: said and counted, refused
 * in strict mode) and the stand-in figure with it (`placeholder` 1), so
 * a converted map whose creatures name rows it didn't bring still shows
 * them standing (1109 step 1q, decision 132); with no resolver, strict
 * mode, or no stand-in, W_ERR_NOT_FOUND naming the table and row.
 * W_ERR_NOT_FOUND when CreatureModelData names no path, or no place has
 * the model and there is no stand-in (no resolver, or no baseline set). */
WErr figure_model_for_display(Resolver *assets, MpqChain *chain, Tables *tables, int display, FigureModel *out, WWhy *why);

/* The baseline set's stand-in figure alone (the player's, when her
 * character can't be made: a test map with no install). W_OK, or
 * W_ERR_NOT_FOUND when no place has it. `why_text` (may be NULL) is kept
 * as stood_in_for. */
WErr figure_model_stand_in(Resolver *assets, MpqChain *chain, const char *why_text, FigureModel *out, WWhy *why);

void figure_model_free(FigureModel *m);

#endif
