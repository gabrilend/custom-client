# 041-figure

*Written by `scripts/generate-info` from `041-figure.h`; don't edit, re-run it.*

041-figure.h (issues 173i, 173j) - an animated model in the world

For a general audience: a "figure" is one animated model drawn in the
world: her, another player's race, or a creature. It holds the model's
pieces (one mesh and picture each), its points as made, and its
skeleton and animations (023-anim.h). Each frame its bones are posed (by a
MovesState, 030-moves.h) and the poses sent to the graphics card, which
bends the points to follow (173u: the meshes never change).

Where a figure's pictures come from:
  a character (figure_from_character): its finished skin sheet and its
    hair, as 025-character.c paints them;
  a creature (figure_from_display): the creature's display row in the
    client's CreatureDisplayInfo table names its model (through
    CreatureModelData) and up to three skins; a model's pictures of kind
    11, 12 and 13 are those skins, found in the model's own folder. The
    rows come through the tables (074-tables.h): a converted map's own
    rows first, then the archives'. A model named .mdx is read as
    named first: a Warcraft III model a converted map brought (its
    folder, through the resolver) is read by 061-mdx and stands as
    made (its skeleton is 402's), its points turned from WC3 units
    into yards, its pictures named inside it through the resolver (a
    missing one is the resolver's checker), a replaceable picture
    (team colour) left off until 404 paints it; any other name, or a
    .mdx no place has, is the game's .m2 beside the name, as the
    game's own rows mean it. A
    humanoid display (a townsperson: its row names a
    CreatureDisplayInfoExtra row) is built as a character instead, with
    that row's race, choices and clothes, wearing its baked skin sheet
    (173t);
  any model: pictures of kind 0 are named inside the model.

Every file comes through the resolver when one is open (a test-map
run: 060's places, the baseline set among them), else the archives
(1109 step 1l; which model a display means is 088-figure-model.h's).
A creature whose model no place has, or a humanoid that can't be made
(no install), is the baseline set's stand-in figure when the set is
there, said; else left out, as before. In a run with a resolver, a
display or look row no place has is a placeholder row (074, filled
from 097's layout), so the creature is that stand-in too, said (1109
step 1r, decision 137).

## What it offers

- `type FigurePiece`
  Yards per Warcraft III unit: W02's rule (world-edit-to-execute's
  wc3_to_wow, map.lua's scale; 1 / 30.72), until a test map says its own.
- `typedef struct Figures Figures;`
  A figure maker (the handle: docs/001, "Handles"): the archives figures
  are read from, the resolver (NULL: the archives alone) and the tables
  (074-tables.h) a creature's rows come from, and the pictures already
  sent to the card, shared by every figure it makes (a picture is read
  and sent once; eleven sentinels share one set of hair). Its pictures
  last until figures_free, so the figures it made must be freed first;
  the chain, resolver and tables must outlive it.
- `Figures *figures_new(MpqChain *chain, Resolver *assets, Tables *tables, Textures *textures);`
- `void figures_free(Figures *fs);`
- `void figures_wc3_scale(Figures *fs, double yards_per_unit);`
  A WC3 model's yards per unit from now on (above 0; else unchanged).
- `type FigureItem`
  an item hung on the figure (173p): its own figure, the bone it hangs
  from and where on it (the model's attachment point), and its place in
  the wearer's model space this frame (figure_bend)
- `type Figure`
- `WErr figure_from_character(Figures *fs, Figure *f, const CharacterLook *look, WWhy *why);`
- `WErr figure_from_display(Figures *fs, Figure *f, int display_id, WWhy *why);`
- `WErr figure_stand_in(Figures *fs, Figure *f, const char *name, WWhy *why);`
  The baseline set's stand-in figure (088, 085: a column a person's
  height, one still bone, Stand), named `name` in messages: the player's
  in a test map whose character can't be made (no install), and a
  creature's whose model no place has (figure_from_display uses it then,
  said). W_ERR_NOT_FOUND when no place has it (no resolver, or no set).
- `WErr figure_from_item(Figures *fs, Figure *f, const char *model, const char *picture, WWhy *why);`
  An item's model (a helm, a shoulder, a weapon), its pictures named
  inside it or the item's own picture (kind 2); hung on a character
  figure by figure_from_character (173p).
- `void figure_bend(Figure *f, const MovesState *moves, uint32_t global_ms);`
  Poses the bones by `moves` and bends the points into the meshes.
- `void figure_draw(const Figure *f, Shader shader, Matrix place);`
  Draws it at `place` (turn and position; the figure's own scale is
  applied inside) with `shader` (043-main.c's model shader: texture0, and the
  uniforms `tint`, `mode` and `unlit`). Two passes, as the doodads': solid
  and cut-out pieces (blend 0-1) first, writing depth; then see-through
  (2) and added-light (3 and up) pieces over them without writing depth
  (an eye's glow, a weapon's shine), each tinted by its animated colour
  and opacity (anim_unit_tint) at the moment it was bent to.
- `Shader figure_shader(void);`
  The shader figure_draw expects (410: shared by the client and the
  dressing room): bends the points by the figure's bones on the card,
  lights them by one sun, fades them into the fog. Its uniforms: `eye`
  (vec3, the camera, for the fog's distance), and those 037-sky.c's
  sky_light_shader sets (sun_dir, sun_col, shade_col, fog_col,
  fog_range); `tint`, `mode` and `unlit` are set per piece by
  figure_draw. Needs the window open; freed with UnloadShader.
- `void figure_free(Figure *f);`
