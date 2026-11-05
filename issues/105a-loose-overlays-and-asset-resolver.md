# 105a — Loose Overlays and the Asset Resolver

*Built ahead of the first replacement (docs/012 had it wait for one), as
background work the owner asked for on 2026-11-04: "build it as best you
can, answer open questions yourself, record each decision".*

## Current behavior

Built (2026-11-04), steps 1 to 6; the Phase 1 demo's table and the
drawing parts asking the resolver are still to come:

- **`src/059-loose-folders.c`** (in the reader library, with the rest
  of 104's file access when it comes):
  - the folder index issue 104 names (`fs_index_folder`,
    `fs_index_find`): every file under a folder, followed through links
    32 folders deep at most, listed under its name folded to small
    letters with forward slashes, so `creature\wolf\wolf.m2` finds a
    pack's `Creature/Wolf/Wolf.M2`. Two names differing only by
    capitals are refused, both named. `fs_normalise` folds a name. Only
    this much of 104 is built: no `fs_read_all`, streams or allocator
    hook yet.
  - the pack manifest's reader (`pack_manifest_read`, `_parse`): a
    `manifest.lua` of plain Lua data (strings with Lua's escapes, long
    brackets, numbers, true, false, nil, tables, both kinds of comment),
    read without running Lua; anything else (a name, a call, a sum) is
    refused with its line. Each entry's fields are held to their type
    and range; a field the format doesn't have is refused by name.
  - the asset kinds (model, texture, animation, particle, sound, font,
    interface; table and ground since 1109's step 1b) and their names.
- **`src/060-resolver.c`** (in the executable and the test program, not
  the library): `resolver_open` indexes every folder and reads every
  pack's manifest at the start, checking each entry's file is in its
  pack; `asset_resolve` asks packs, the server's folders, overlays, the
  archives, then the built-in placeholder, and hands back the bytes, the
  place, the folder or archive, and a pack's manifest entry; a trace
  line per answer names the winner. Placeholders are built into the
  binary: an 8 x 8 magenta and black BLP2, a box M2 (version 264) with
  its .skin, a 0.2 s WAV beep, and empty answers for an animation and a
  font. Each placeholder is a warning naming the asset and where it was
  looked for; in strict mode it is `W_ERR_NOT_FOUND` and
  `resolver_exit_status` turns to 1. `asset_counts` and
  `resolver_report` give the table: different assets per kind and
  place, and per kind how many pack ones are past the threshold. One
  lock covers the counts, so the reading-ahead thread may ask too.
  `resolver_place_from_settings` builds the places from the settings.
- **Settings** (`src/014-settings.c`, section Assets): `assetPacks`,
  `assetOverlays` (folder names under the profile's `assets/packs` and
  `assets/overlays`, commas between), `assetsStrict` (off),
  `assetsDistinctness` (0.5).
- **`scripts/build`**: 059 joins the reader library's list, 060 the
  core list, so the test program has both.
- **docs/012**: the resolver's five places, and the manifest's format
  ("The pack manifest").

Checked (`tests/044-test-client.c`, "the asset resolver", 58 checks,
run before the archives so it needs no game data): a made-up archive
written by the test (stored files, its tables scrambled as the game's
are) opened as a chain; a pack file replacing the archive's file of the
same path, in another spelling, with its manifest entry; the first pack
outranking the second; an overlay beating the archive; the archive
answering when no folder has the file; a missing model, picture, sound
and font each getting its placeholder and a warning, the box read by
022-m2 and the checker decoded by 017-blp; strict mode refusing a
placeholder and turning the exit status to 1; counts once per asset,
four threads asking at once; a pack with no manifest, an entry whose
file is missing, a missing folder; the folder index (104's
`Creature/Wolf/Wolf.M2` found by `creature\wolf\wolf.m2`; two names
differing by capitals refused); a full manifest read field by field,
and thirteen kinds of broken manifest refused, each with its code and
sentence; the settings and the places made from them. 58 pass; the
same with the address and undefined-behaviour checkers
(`-fsanitize=address,undefined`, leaks checked) and with the race
checker (gcc's `-fsanitize=thread`): clean. The manifest example in
docs/012 loads in LuaJIT with an empty environment, as
world-edit-to-execute would read it.

Built and checked away from the owner's machine: raylib isn't installed
there, so `scripts/build` stops at its first check. The test program was
built with `scripts/build`'s own compiler line and core list against
stand-in `raylib.h` and `raymath.h` holding only the types and maths it
uses (`032-sight.c`, a core part, and the test program itself include
them); 059 and 060 include neither. With no game files there, the
program stops at "the archives", as it always does without them, after
the resolver's checks have run. `scripts/build --test` on the owner's
machine is the full check.

How it was before: nothing decided where an asset comes from. Every
drawing part opened the archives itself, there was no way to put a
replacement in front of a Blizzard file or load a converted WC3 map
from a folder, and nothing counted how much was still borrowed.

Not built yet:
- **The Phase 1 demo printing the table** (acceptance line 2):
  `resolver_report` prints it; the demo itself is
  [108](./108-phase-demo.md), not written yet, and calls it there.
- **Drawing parts asking the resolver**, in part. Since 1109's step 1b
  (2026-11-04) the ground's files (the WDT and ADTs in 033 and 034, the
  WDL in 039), the client's tables (074, per row: 106) and a creature
  look's own WC3 model and its pictures (041) ask it, with the
  archives alone when there is no resolver. Two kinds were added for
  them, `table` and `ground`, with no placeholder (no place having one
  is the answer, quietly: a made-up table or ground tile would be
  wrong), and two calls: `asset_resolve_all` (every place's copy, for a
  table read per row) and `asset_read` (the first copy, never a
  placeholder, through the resolver when one is open, else the
  archives). Since 1109's step 1c (2026-11-05) the ground's pictures
  (033, 038) and the doodads' and buildings' models, pictures and
  `.anim` files (042, through `src/076-placed-models.c`) ask it too,
  with two more calls: `asset_get` (the placeholder when there is a
  resolver, the archives alone when there isn't) and `asset_stand_in`
  (the placeholder for all of a model's files at once, when one of
  them is found and another isn't). A box stands in as a magenta
  wire-frame (step 4's wire-frames, for doodads; a missing building is
  the box at the size its placement gives). Still reading the archives
  directly (`mpq_chain_read`): 034's prefetch, 040's water, and 041's
  pictures and models for the game's own creatures; each is the next
  slice. The main
  program (043) opens a resolver only in a test-map run (1109's
  decision 4), so an ordinary run's exit status doesn't reflect strict
  mode yet; when it does, it exits with `resolver_exit_status`
  (acceptance line 3, which the test checks on the resolver).
- **The server's delivered assets**: their folder list is in the place
  (`server`) and searched second, but nothing fills it until
  world-edit-to-execute's 604/605/609 deliver into a folder (docs/012,
  open question 6).
- **Checking each pack file's SHA-256 against its `id`** (open question 1
  below).
- **Packs as archives**: a pack is a folder here; a pack arriving as an
  archive (docs/012, "How players share assets") is unpacked into one
  first, which needs 105's writer and an unpacker of a player's archive.

## Intended behavior

One function every consumer calls:

```c
WErr asset_resolve(Resolver *r, const char *logical, AssetKind kind,
                   AssetAnswer *out, WWhy *why);   /* bytes, source, folder or archive, pack entry */
void asset_counts(Resolver *r, AssetCounts *out); /* per source, per kind */
```

Lookup order (changed 2026-11-04, decision 1 below: the server's
delivered assets added as the second place, from docs/012):

1. **Replacement packs** (`assetPacks`, highest priority first): loose
   folders with a manifest listing each file's provenance, similarity score and
   seal ([docs/012](../docs/012-asset-replacement-and-provenance.md)).
2. **The server's delivered assets** (docs/012, open questions 3 to 6).
3. **Overlays** (`assetOverlays`): loose folders such as a converted WC3
   map's terrain and placements from world-edit-to-execute.
4. **The MPQ chain** ([105](./105-mpq-reader.md)).
5. **Placeholder**, chosen by kind: magenta checker texture, box model
   (drawn as a wireframe by whoever draws it), short beep. A warning
   each time (named, counted); an error when `assetsStrict` is on.

`AssetCounts` holds, per asset kind (model, texture, animation, particle,
sound, font, interface), a count per source (uint32) and, for packs, how many
are past the distinctness threshold (uint32). Every phase demo prints it.

The resolver lives in the executable, not in `libwreaders.so`:
world-edit-to-execute has its own resolver (its issue W03a) because its
lookup rules for WC3 unit types differ, but both read the same pack manifest
format.

## Suggested implementation steps

1. ~~Pack manifest format (plain text or Lua table; one entry per file:
   logical path, file, provenance id, latest score, seal flag).~~
   Done: a Lua table of data, `manifest.lua`; each entry the whole
   provenance record (decision 2).
2. ~~Index packs and overlays at startup with `fs_index_folder` ([104](./104-fs-abstraction.md)).~~
   Done: 104 had no folder index, so its `fs_index_folder` was built
   here, in 059, and nothing else of 104.
3. ~~`asset_resolve` walking the four steps; log a `trace` line per resolution
   naming the winner.~~ Done, five steps; trace on `ResolverPlace.trace`
   (102's log levels don't exist yet).
4. ~~Placeholder assets built into the binary (no files needed).~~ Done.
5. ~~Counters, and a `resolver_report` that prints the table.~~ Done.
6. ~~Tests: a fixture pack overriding a fixture chain file; an overlay file
   beating the chain; a missing file producing a placeholder and a warning; strict mode producing an error.~~
   Done, with the fixture chain a real archive written by the test.

## Acceptance

- [x] A pack file replaces the Blizzard file of the same logical path.
- [ ] The counts table is printed by the Phase 1 demo. (`resolver_report`
  prints it; waits for [108](./108-phase-demo.md).)
- [x] Strict mode turns any placeholder into a non-zero exit. (The
  resolver's exit status, checked in the tests; the client's main takes
  it when it opens a resolver.)

## Decisions (2026-11-04, answered here; the owner may overturn any)

1. **Five places, not four.** The issue listed packs, overlays, the
   archives, the placeholder; docs/012 lists packs, the server's assets,
   the archives, the placeholder. Both kinds of folder are kept: packs,
   then the server's, then overlays, then the archives. The server comes
   before overlays because it holds the truth about the world it runs
   (docs/001, "Who holds the truth"); the player's own packs outrank
   both (the owner, 2026-10-09: "player chosen, server supplied,
   blizzard original"). Each is counted under its own name, so the
   table has five columns.
2. **The manifest is a Lua table, `manifest.lua`, of plain data.**
   world-edit-to-execute defines no pack manifest yet (its 504 is open
   and its W03a not written), but every manifest it writes is a
   `manifest.lua` returning a table (its patch layers, read with
   `loadfile`), and the owner prefers Lua. So the format matches that
   shape. The client reads it with its own data-only reader rather than
   linking LuaJIT (not in the client yet) or running a file a stranger
   sent: no names, calls or sums are taken. Each entry is the whole
   provenance record of docs/012 (id, replaces, route, lineage,
   licence, generation, scores, seal, transcript) plus `file` and
   `kind`, rather than only the latest score, so the asset card can show
   all of it and the history stays with the file.
3. **Unknown fields are refused; `wc3` is set aside.** Warnings are
   errors here, and a misspelt `linage` silently dropped would lose the
   lineage claim. A later layout gets a new `format` number
   (`W_ERR_UNSUPPORTED` here until this reader learns it). One table,
   `wc3`, is left for world-edit-to-execute's own override fields (its
   504: scale, tint, team colour, every map or one), unread here.
4. **The seed is written as a string of digits.** docs/012 has it as a
   uint64; LuaJIT's numbers hold 53 bits exactly, so a number would be
   rounded on world-edit-to-execute's side.
5. **The seal is `false` or `{ build, date }`.** docs/012 calls it a
   bool; world-edit-to-execute's W05c stores the date and the stock
   client's build with it. Plain `true` is refused, so a seal always
   says what it was checked against.
6. **Scores are listed oldest first; the latest is the last.** Dates
   out of order are refused. An entry with no scores counts as not past
   any threshold.
7. **"Past the threshold" means at or above it, and the threshold is
   0.5 for now** (`assetsDistinctness`). world-edit-to-execute's open
   question 10 (which number) is still the owner's; 0.5 is a stand-in
   that changes in one setting. A pack's file is served whatever its
   score: the score decides only whether it counts as replaced.
8. **The `id` is checked for shape, not against the file.** Hashing
   every pack file at every start costs time a large pack would feel;
   a check of each file's SHA-256 against its `id` (a changed file
   means its scores are stale) belongs in a separate checking run or in
   world-edit-to-execute's tools. Left open below.
9. **Packs and overlays are named, not given as paths.** The settings
   store holds 63 characters a value, and a profile's settings should
   never point outside its own assets: `assetPacks = "wolves,cats"`
   means `assets/packs/wolves` and `assets/packs/cats`. Names of
   letters, digits, `-`, `_` and `.`, never `.` or `..`. The resolver
   itself takes full paths (`ResolverPlace`), so tests and other
   programs can point anywhere.
10. **Only files an entry names are served from a pack.** A pack file
    with no entry has no provenance; it is named once at the start
    ("never served") rather than refused, so a pack can carry a readme.
11. **A pack entry of another kind than asked for is refused**
    (`W_ERR_REFUSED`, both kinds named), not served: one of the two is
    wrong, and guessing hides which. Refused requests aren't counted.
12. **The placeholders are real files of the readers' formats**, so a
    consumer reads a placeholder exactly as it reads anything else; the
    answer's `source` says it is one, for a part that wants to draw the
    box as a wireframe. A `.skin` name gets the box's skin. An animation
    and a font get empty answers (the resting pose; the built-in font),
    since nothing reads either kind through the resolver yet.
13. **Counts are of different assets** (one wolf asked for by a
    thousand doodads is one), as docs/012's "how many assets drawn this
    session came from each source"; the placeholder warnings are counted
    every time as well, since each is said every time.
14. **The trace is a switch on the resolver** (`trace`), writing to its
    log (stderr unless given): 102's logging levels aren't built.
15. **The folder index is followed through links, 32 folders deep at
    most**: the owner's assets folder holds links
    (`assets/server-data`), and a link looping back stops at the depth
    limit with an error rather than running forever.
16. **059 goes into the reader library, 060 doesn't.** 104 puts file
    access in `libwreaders.so`, and world-edit-to-execute may want the
    same case-folded index and the same manifest checks from LuaJIT;
    the resolver itself is the executable's, as this issue says.

## Open questions

1. **Check pack files against their `id`?** At every start (slow for a
   big pack), on request, or only in world-edit-to-execute's tools?
   Until answered: shape only (decision 8).
2. **The distinctness threshold's number**: world-edit-to-execute's open
   question 10. 0.5 until then.

## Related

- [docs/012-asset-replacement-and-provenance.md](../docs/012-asset-replacement-and-provenance.md)
- Blocked by: [104](./104-fs-abstraction.md) (its folder index built here), [105](./105-mpq-reader.md).
- Blocks: [108](./108-phase-demo.md), [204](./204-blp-to-gpu-upload.md), every consumer of assets.
- world-edit-to-execute: `issues/W03-show-wow-models-with-wc3-unit-behavior.md`
  (W03a, its own resolver), `issues/504-create-asset-pack-specification.md`
  (its override fields, under `wc3`), `issues/W05c-default-client-compatible-seal.md`.
