# 060-resolver

*Written by `scripts/generate-info` from `060-resolver.h`; don't edit, re-run it.*

060-resolver.h (issue 105a) - where each asset comes from

For a general audience: every model, picture, animation, particle
effect, sound, font and interface picture the client uses is asked for
by its name in the original game ("Creature\Wolf\Wolf.m2"), and this
decides which copy answers. It asks five places in order and the first
that has the file wins:

  1. the player's replacement packs, highest priority first: folders
     with a manifest giving each file's provenance, similarity score
     and seal (059-loose-folders.h; docs/012);
  2. the assets a server delivered (docs/012, open questions 3 to 6:
     fetched before logging in into a folder on disk; none yet);
  3. overlays: loose folders such as a Warcraft III map converted by
     world-edit-to-execute (its terrain and placements);
  4. the player's own Blizzard install: the original game's archives
     (015-mpq.h), then, when they plugged it in, their Warcraft III
     archives (war3.mpq, War3x.mpq, War3Patch.mpq: the `assetWc3`
     setting), for the WC3 pictures and models a converted map names;
  5. a placeholder built into the client, chosen by kind: a magenta
     and black checker picture, a box model, a short beep. Each time
     one stands in, a warning names it. With strict mode on it is an
     error instead, and the program's exit status says so. A table or
     a map's ground file has none: no place having it is the answer
     (W_ERR_NOT_FOUND, no warning).

A table is read per row (issue 106): every place's copy is asked for
(asset_resolve_all), and a row in an earlier place wins over the same
row in a later one (074-tables.h).

It counts, per kind and per place, how many different assets this
session asked for, and how many pack assets are past the distinctness
threshold (their latest score at or above it), so every phase demo and
the HUD can show how much is still borrowed. A pack whose entry
replaces a path wins over the archives' file of that path whatever its
score: the score only decides whether it counts as replaced.

The placeholders' bytes are in the formats the readers take: the
checker is a BLP2 (8 x 8, plain pixels: 017-blp.h decodes it), the box
an M2 of version 264 with its .skin (022-m2.h reads them; a static box
a yard high, no bones), the beep a WAV (0.2 s at 880 Hz, 16-bit, 22050
a second). An animation's placeholder is empty (the model's resting pose) and
a font's is empty (the client's own built-in font). A particle effect
stands in as the box. Which of the box's two files is given goes by
the name: a name ending in .skin gets the skin.

Window-free: in the test program and the executable; not in the reader
library, since world-edit-to-execute has its own resolver for WC3 unit
types (its W03a) and reads the same manifests (059).

## What it offers

- `type AssetSource`
- `const char *asset_source_name(AssetSource source);`
  The place's name for reports ("pack", "server", "overlay", "blizzard", "placeholder").
- `type ResolverPlace`
  Where the resolver looks, and how it reports:
    packs, server, overlays  folders, highest priority first in each list
    chain       the original game's archives; NULL for none (a test, or a
                player with no copy: everything then comes from the folders)
    wc3         a folder holding the player's Warcraft III archives (or a
                link to their install), asked after `chain`; "" for none.
                The resolver opens them itself (mpq_chain_open_wc3); a
                folder with none of them is said and left out
    strict      1: a placeholder is an error (the `assetsStrict` setting)
    distinctness  a pack asset counts as replaced from this score up
                (0 to 1; the `assetsDistinctness` setting)
    log         where warnings and trace lines go; NULL for stderr
    trace       1: a line for every asset asked for, naming the place that answered
- `WErr resolver_place_from_settings(const Settings *st, const char *assets_dir, MpqChain *chain, ResolverPlace *out, WWhy *why);`
  The place from the settings store: `assetPacks` and `assetOverlays`
  name folders under `assets_dir`/packs and `assets_dir`/overlays
  (commas between names, highest priority first), `assetsStrict` and
  `assetsDistinctness` as above, `wc3` the folder `assetWc3` names under
  `assets_dir` (none when either is empty); `chain` as given; no server
  folders yet, log stderr, trace off. W_OK (folders are checked when
  opened).
- `typedef struct Resolver Resolver;`
  The resolver (the handle: docs/001, "Handles"). Indexes every folder
  (059's fs_index_folder) and reads each pack's manifest now, so a broken
  pack stops the start, by name, not the first frame that meets it.
  W_OK with `*out`; otherwise the code of what failed, with the folder:
  W_ERR_NOT_FOUND for a folder missing, a pack with no manifest.lua, or an
  entry whose file isn't in its pack; the manifest's own codes (059); a
  folder's own (two names differing by capitals: W_ERR_REFUSED); a
  Warcraft III archive's own code when one is there but can't be read
  (no folder, or none of its archives, is only said in the log). The
  chain must outlive it. Safe to ask from more than one thread.
- `WErr resolver_open(const ResolverPlace *place, Resolver **out, WWhy *why);`
- `void resolver_free(Resolver *r);`
- `type AssetAnswer`
  An answer: the bytes (the caller's, freed with asset_answer_free), the
  place, which folder or archive (the resolver's or the chain's words,
  valid while they live), and for a pack its manifest entry (provenance,
  scores, seal; NULL from anywhere else).
- `WErr asset_resolve(Resolver *r, const char *logical, AssetKind kind, AssetAnswer *out, WWhy *why);`
  The asset `logical` (any capitals, either slash) of `kind`. W_OK with
  `*out`, a placeholder included (with its warning) unless strict:
  then W_ERR_NOT_FOUND, no bytes, and the exit status turns to 1.
  W_ERR_REFUSED for an empty name, a kind outside the list, or a pack
  entry of another kind under that name; W_ERR_IO when a folder's file
  can't be read. An answer is counted (asset_counts) when it is given,
  and a placeholder when it stands in or strict mode refuses it; a
  refused request is not.
- `void asset_answer_free(AssetAnswer *a);`
- `WErr asset_resolve_all(Resolver *r, const char *logical, AssetKind kind, AssetAnswer *out, int most, int *n, WWhy *why);`
  Every copy of `logical` of `kind`, the first place first: each pack,
  server folder and overlay that holds it, then the archives' (the chain
  gives one copy, its highest patch's). Never a placeholder. W_OK with
  `*n` answers in `out` (0 when no place has it), each freed with
  asset_answer_free; W_ERR_FULL when more than `most` places hold it;
  asset_resolve's refusals and W_ERR_IO. Counted as asset_resolve counts.
- `WErr asset_read(Resolver *r, MpqChain *chain, const char *logical, AssetKind kind, AssetAnswer *out, WWhy *why);`
  For readers that run with or without a resolver (one opens only in a
  test-map run, issue 1109's decision 4), and for a reader that tries
  one name and then another: the first place's copy, as asset_resolve
  finds it when `r` is given (else the archives alone, `chain`), but
  never a placeholder: W_ERR_NOT_FOUND, quietly, when no place has it.
- `WErr asset_get(Resolver *r, MpqChain *chain, const char *logical, AssetKind kind, AssetAnswer *out, WWhy *why);`
  For readers that draw (pictures, models, buildings; issue 1109's step
  1c): asset_resolve's answer, a placeholder included, when `r` is given;
  with no resolver (every run but a test map's, 1109's decision 4) the
  archives alone, `chain`, as those readers read before: W_ERR_NOT_FOUND,
  quietly, when they lack it.
- `WErr asset_stand_in(Resolver *r, const char *logical, AssetKind kind, AssetAnswer *out, WWhy *why);`
  The built-in placeholder for `logical` of `kind`, as asset_resolve's
  last place gives it: said in a warning, counted, refused in strict mode
  (W_ERR_NOT_FOUND), and W_ERR_NOT_FOUND quietly for a kind with none.
  For an asset of several files one of which no place has (a model's .m2
  found but not its .skin, a building's root that isn't anywhere): the
  placeholder stands in whole, rather than one file of it beside another
  model's.
- `type AssetCounts`
  The session's counts: different assets asked for, per kind and place
  (an asset asked for again isn't counted again); per kind, how many of
  the pack ones are past the distinctness threshold; and how many times
  a placeholder stood in (or would have, in strict mode), every time.
- `void asset_counts(Resolver *r, AssetCounts *out);`
- `void resolver_report(Resolver *r, FILE *to);`
  Prints the counts as a table, one row a kind and a total, with the
  threshold and strict mode, to `to`.
- `const char *resolver_wc3(const Resolver *r);`
  The folder Warcraft III's archives were opened from, or NULL when the
  resolver has none (no folder given, or none of its archives there).
- `int resolver_exit_status(Resolver *r);`
  What the program should exit with for its assets: 1 when strict mode
  is on and any placeholder was asked for, else 0.
