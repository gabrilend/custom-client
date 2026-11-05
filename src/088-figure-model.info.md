# 088-figure-model

*Written by `scripts/generate-info` from `088-figure-model.h`; don't edit, re-run it.*

088-figure-model.h (issues 173t, 1109) - which files a figure is made from

For a general audience: before a creature (or, in a test map with no
install, the player) can be drawn, the client must find its model.
This finds it, with no window, the way 041-figure.c then builds it: a
creature's display row (CreatureDisplayInfo, through the tables: a
converted map's own rows first) names its size, its skins and, through
CreatureModelData, its model; a humanoid row names a character look
instead (built by 025, which still needs the game's archives). A model
named .mdx that a converted map brought is read as named (Warcraft
III's MDLX); any other is the game's .m2 and its first view, from the
first place that has them (060: packs, the map's folder, the
installs, the baseline set). When no place has them and a resolver is
open, the baseline set's stand-in figure answers (085: a column a
person's height with one still bone and a Stand animation, its face
along the way it faces), said, so the creature still stands where it
should; it answers too where only the set answers the model (the set
stands its skeleton-less post in for every model the converter named,
and a creature needs a skeleton to stand and walk with). With no
resolver, or no set, the model is not found, as before.

Window-free (CORE: the test program and the executable); 041 builds
what it finds.

## What it offers

- `type FigureModelKind`
- `type FigureModel`
  What was found (owned until figure_model_free):
    kind, display  what it is, and the display row asked for (0 for the player's stand-in)
    scale          the display's size (1 when the row gives none sensible)
    extra          HUMANOID: its CreatureDisplayInfoExtra row
    named          the model CreatureModelData names ("" for the player's stand-in)
    base           the model read, without its extension (GAME, STAND_IN):
                   its skeleton is in base.m2, its .anim files beside it
    folder         the named model's folder, with its last backslash (its skins)
    skins          the display's three skin names (pictures of kind 11 to 13)
    model, skin    the model's bytes (MDLX for WC3) and, but for WC3 and
                   HUMANOID, its view (.skin), with where each came from
    stood_in_for   STAND_IN: what no place had, said
- `WErr figure_model_for_display(Resolver *assets, MpqChain *chain, Tables *tables, int display, FigureModel *out, WWhy *why);`
  A creature display's model. W_OK with `*out`; the tables' refusal
  (W_ERR_NOT_FOUND naming the table and row) when a row is missing;
  W_ERR_NOT_FOUND when CreatureModelData names no path, or no place has
  the model and there is no stand-in (no resolver, or no baseline set).
- `WErr figure_model_stand_in(Resolver *assets, MpqChain *chain, const char *why_text, FigureModel *out, WWhy *why);`
  The baseline set's stand-in figure alone (the player's, when her
  character can't be made: a test map with no install). W_OK, or
  W_ERR_NOT_FOUND when no place has it. `why_text` (may be NULL) is kept
  as stood_in_for.
- `void figure_model_free(FigureModel *m);`
