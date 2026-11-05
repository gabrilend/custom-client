# 059-loose-folders

*Written by `scripts/generate-info` from `059-loose-folders.h`; don't edit, re-run it.*

059-loose-folders.h (issues 104, 105a) - folders of loose files, and a replacement pack's manifest

For a general audience: besides the game's archives, the client reads
assets from plain folders: a player's replacement packs (their own
models, pictures and sounds standing in for the original game's), a
server's delivered assets, and converted Warcraft III maps from
world-edit-to-execute. The game names its files with backslashes and
any mix of capitals ("Creature\Wolf\Wolf.m2"), but a folder on Linux
tells "Wolf.M2" and "wolf.m2" apart. So each folder is walked once
when it is opened, and every file in it is listed under its name in
small letters with forward slashes; a request in any spelling finds
it. Two files whose names differ only by capitals are refused, naming
both, rather than one picked silently.

A replacement pack also holds a manifest, `manifest.lua`: for each
file, which original it replaces and the record of how it was made
(docs/012-asset-replacement-and-provenance.md: provenance, similarity
scores, the "default client compatible" seal). It is a Lua table of
data, so world-edit-to-execute reads the same file with its own Lua
(loadfile, with an empty environment) and this client reads it with
the small data-only reader here: strings, numbers, true, false and
tables; comments; no names, calls or arithmetic.

The manifest, format 1 (every key below; any other key is refused, so
a misspelt field is caught, except `wc3`):

  return {
      format  = 1,                            -- this layout's number
      name    = "Wolves, redrawn",            -- the pack's name, for reports
      entries = {
          {
              replaces = "Creature\\Wolf\\Wolf.m2",  -- the logical path it answers (the original's)
              file     = "wolf/wolf.m2",      -- the file in this folder (any capitals, either slash)
              kind     = "model",             -- model texture animation particle sound font interface table ground
              id       = "3a7bd3e2...",       -- SHA-256 of the file's bytes, 64 small hex digits
              route    = "generate",          -- search generate hand restyle
              lineage  = "independent",       -- derived independent
              licence  = "CC-BY-4.0, A. Maker, https://...",  -- needed when route is search
              generation = {                  -- needed when route is generate
                  prompt = "a grey wolf, low poly", seed = "18446744073709551615",
                  workflow = "<hash of the workflow>", models = { "model name", ... },
              },
              scores = {                      -- oldest first; the last is the latest
                  { score = 0.41, date = "2026-10-30", by = "W05a outline + embedding" },
              },
              seal = false,                   -- or { build = 12340, date = "2026-11-01" }
              transcript = "llm-transcripts/nov-01-26.md:120-180",
              wc3 = { ... },                  -- world-edit-to-execute's own fields (its 504); not read here
          },
      },
  }

Required in an entry: replaces, file, kind, id, route, lineage. A
score is 0.0 (identical to the original) to 1.0 (unrelated); an entry
with no scores yet has none past any threshold. A seed is written as
a string of digits because it is a 64-bit number and LuaJIT's numbers
hold only 53 bits exactly. Dates are YYYY-MM-DD.

## What it offers

- `typedef struct FsIndex FsIndex;`
  A folder's files by folded name (the handle: docs/001, "Handles"):
  read-only once made, so any number of threads may look up at once.
- `WErr fs_index_folder(const char *root, FsIndex **out, WWhy *why);`
  Walks `root` and everything under it (folders followed through links,
  32 deep at most) and lists each file. W_OK with `*out`; W_ERR_NOT_FOUND
  when `root` is not a folder; W_ERR_REFUSED for two names that differ
  only by capitals (both named); W_ERR_IO when a folder can't be read.
- `void fs_index_free(FsIndex *ix);`
- `const char *fs_index_find(const FsIndex *ix, const char *logical);`
  The file's full path on disk for `logical` in any spelling, or NULL when the folder has none.
- `int fs_index_count(const FsIndex *ix);`
  How many files, and the i-th (in folded-name order): its folded name, and its full path in `*real` when `real` is not NULL.
- `const char *fs_index_at(const FsIndex *ix, int i, const char **real);`
- `int fs_normalise(const char *in, char *out, size_t n);`
  `in` folded into `out` (`n` bytes): small letters, `/` for `\`, no
  doubled or leading slashes, no "./" parts. 0, or 1 when it doesn't fit
  or is empty once folded.
- `type AssetKind`
  {{{ the kinds of asset (docs/012, "The resolver")
  table: one of the client's tables (DBFilesClient\Map.dbc), read per
  row with the first place first (issue 106; 074-tables.h). ground: a
  map's ground files (its .wdt, .wdl and each tile's .adt). Neither has
  a placeholder: a table no place has is an error for its reader, a
  ground file no place has is open sea.
- `const char *asset_kind_name(AssetKind kind);`
  The kind's name as the manifest writes it ("model"), and back: -1 for a name that isn't a kind.
- `int asset_kind_from_name(const char *name);`
- `type PackRoute`
  {{{ the pack manifest
- `type PackLineage`
- `type PackScore`
- `type PackEntry`
- `type PackManifest`
  A pack's manifest, read (the handle); every string lives until pack_manifest_free.
- `WErr pack_manifest_parse(const char *text, size_t len, const char *where, PackManifest **out, WWhy *why);`
  Reads a manifest from `text` (`len` bytes; `where` names it in
  sentences, "packs/wolves/manifest.lua"). W_OK with `*out`; W_ERR_CORRUPT
  for text that isn't the Lua data above (with its line); W_ERR_UNSUPPORTED
  for a format other than 1; W_ERR_REFUSED for a field missing, unknown,
  of the wrong type or out of its range, or two entries replacing one
  path.
- `WErr pack_manifest_read(const char *path, PackManifest **out, WWhy *why);`
  The same, from a file: W_ERR_NOT_FOUND when there is none, W_ERR_IO when it can't be read.
- `void pack_manifest_free(PackManifest *m);`
- `double pack_entry_latest(const PackEntry *e);`
  The entry's latest score (its last), or -1 when it has none.
- `type LuaDataType`
  {{{ Lua data, any file of it
  The manifest's reader, for any file of plain Lua data (`return` and
  one value; the same language as above): a converted map's map.lua
  (073-converted-map.h) among them.
- `WErr lua_data_parse(const char *text, size_t len, const char *where, LuaData **out, WWhy *why);`
  Reads `text` (`len` bytes; `where` names it in sentences). W_OK with
  `*out`; W_ERR_CORRUPT for text that isn't Lua data, with its line.
- `WErr lua_data_read(const char *path, size_t most, LuaData **out, WWhy *why);`
  The same from a file of at most `most` bytes: W_ERR_NOT_FOUND when
  there is none, W_ERR_REFUSED when it is larger or not a file, W_ERR_IO
  when it can't be read.
- `void lua_data_free(LuaData *d);`
- `const LuaValue *lua_data_top(const LuaData *d);`
  The value returned; a value's type (NULL is nil) and its type's name
  in sentences ("a number"); the line it starts on (0 for NULL).
- `LuaDataType lua_value_type(const LuaValue *v);`
- `const char *lua_type_name(LuaDataType t);`
- `int lua_value_line(const LuaValue *v);`
- `double lua_value_number(const LuaValue *v);`
  Its number (0 when it isn't one), whether it is true, its text (NULL when it isn't text).
- `int lua_value_true(const LuaValue *v);`
- `const char *lua_value_text(const LuaValue *v);`
- `const LuaValue *lua_value_field(const LuaValue *t, const char *key);`
  A table's field `key` (NULL when it has none, or isn't a table); how
  many values it lists without keys; the i-th of them, from 1 (NULL past the end).
- `int lua_value_listed(const LuaValue *t);`
- `const LuaValue *lua_value_item(const LuaValue *t, int i);`
- `int lua_value_count(const LuaValue *t);`
  Every value a table holds, keyed or not, in the order written: how
  many, and the i-th from 1 (NULL past the end), with its key in `*key`
  (NULL for a value listed without one) when `key` is not NULL.
- `const LuaValue *lua_value_at(const LuaValue *t, int i, const char **key);`
