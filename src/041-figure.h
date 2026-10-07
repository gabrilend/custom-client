/*
 * 041-figure.h (issues 173i, 173j) - an animated model in the world
 *
 * For a general audience: a "figure" is one animated model drawn in the
 * world: her, another player's race, or a creature. It holds the model's
 * pieces (one mesh and picture each), its points as made, and its
 * skeleton and animations (023-anim.h). Each frame its bones are posed (by a
 * MovesState, 030-moves.h) and the poses sent to the graphics card, which
 * bends the points to follow (173u: the meshes never change).
 *
 * Where a figure's pictures come from:
 *   a character (figure_from_character): its finished skin sheet and its
 *     hair, as 025-character.c paints them;
 *   a creature (figure_from_display): the creature's display row in the
 *     client's CreatureDisplayInfo table names its model (through
 *     CreatureModelData) and up to three skins; a model's pictures of kind
 *     11, 12 and 13 are those skins, found in the model's own folder. A
 *     humanoid display (a townsperson: its row names a
 *     CreatureDisplayInfoExtra row) is built as a character instead, with
 *     that row's race, choices and clothes, wearing its baked skin sheet
 *     (173t);
 *   any model: pictures of kind 0 are named inside the model.
 */
#ifndef EG_FIGURE_H
#define EG_FIGURE_H

#include "013-errors.h"

#include "023-anim.h"
#include "025-character.h"
#include "030-moves.h"
#include "015-mpq.h"
#include "raylib.h"

typedef struct {
    Mesh       mesh;
    Texture2D  tex;
    uint32_t  *point_of;              /* per mesh corner: the model point it is */
    uint16_t   blend, flags;          /* the piece's blend mode (022-m2.h: 0 solid ... 4 added light) and render flags (0x1 unlit) */
    int        colour, fade;          /* its animated tint's tracks (022-m2.h's M2Unit), -1 none */
    int        layers, moving_uv;     /* two-picture pieces and moving picture positions (not drawn for figures yet) */
} FigurePiece;

/* an item hung on the figure (173p): its own figure, the bone it hangs
 * from and where on it (the model's attachment point), and its place in
 * the wearer's model space this frame (figure_bend) */
struct Figure;
typedef struct { struct Figure *fig; int bone; float pos[3]; float at[16]; } FigureItem;

typedef struct Figure {
    char         name[64];            /* for messages */
    FigurePiece  pieces[64];  int npieces;
    M2Vertex    *rest;        uint32_t nrest;      /* points and facings as made */
    Anim         anim;
    float      (*poses)[16], (*scratch)[16];
    Texture2D    bones;               /* the bones' poses for the card (173u): one row, four float texels a bone,
                                       * each a column of its matrix; the figure shader bends the points */
    float        scale;               /* the display's size (1 for characters) */
    FigureItem   items[6];  int nitems;   /* helm, shoulders, weapons (173p) */
    int          seq;  uint32_t ms, global_ms;   /* the animation and moment it was last bent to (for the pieces' tints) */
    Texture2D    owned[8];  int nowned;  /* pictures made for this figure alone (painted skin, hair), freed with it; pictures
                                          * read from the archives are shared through a cache and kept (173t) */
} Figure;

WErr  figure_from_character(Figure *f, MpqChain *chain, const CharacterLook *look, WWhy *why);
WErr  figure_from_display(Figure *f, MpqChain *chain, int display_id, WWhy *why);

/* An item's model (a helm, a shoulder, a weapon), its pictures named
 * inside it or the item's own picture (kind 2); hung on a character
 * figure by figure_from_character (173p). */
WErr  figure_from_item(Figure *f, MpqChain *chain, const char *model, const char *picture, WWhy *why);

/* Poses the bones by `moves` and bends the points into the meshes. */
void figure_bend(Figure *f, const MovesState *moves, uint32_t global_ms);

/* Draws it at `place` (turn and position; the figure's own scale is
 * applied inside) with `shader` (043-main.c's model shader: texture0, and the
 * uniforms `tint`, `mode` and `unlit`). Two passes, as the doodads': solid
 * and cut-out pieces (blend 0-1) first, writing depth; then see-through
 * (2) and added-light (3 and up) pieces over them without writing depth
 * (an eye's glow, a weapon's shine), each tinted by its animated colour
 * and opacity (anim_unit_tint) at the moment it was bent to. */
void figure_draw(const Figure *f, Shader shader, Matrix place);

void figure_free(Figure *f);

#endif
