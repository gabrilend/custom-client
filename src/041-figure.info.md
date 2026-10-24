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
    11, 12 and 13 are those skins, found in the model's own folder. A
    humanoid display (a townsperson: its row names a
    CreatureDisplayInfoExtra row) is built as a character instead, with
    that row's race, choices and clothes, wearing its baked skin sheet
    (173t);
  any model: pictures of kind 0 are named inside the model.

## What it offers

- `type FigurePiece`
- `typedef struct Figures Figures;`
  A figure maker (the handle: docs/001, "Handles"): the archives figures
  are read from, and the pictures already sent to the card, shared by
  every figure it makes (a picture is read and sent once; eleven
  sentinels share one set of hair). Its pictures last until figures_free,
  so the figures it made must be freed first.
- `Figures *figures_new(MpqChain *chain, Textures *textures);`
- `void figures_free(Figures *fs);`
- `type FigureItem`
  an item hung on the figure (173p): its own figure, the bone it hangs
  from and where on it (the model's attachment point), and its place in
  the wearer's model space this frame (figure_bend)
- `type Figure`
- `WErr figure_from_character(Figures *fs, Figure *f, const CharacterLook *look, WWhy *why);`
- `WErr figure_from_display(Figures *fs, Figure *f, int display_id, WWhy *why);`
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
