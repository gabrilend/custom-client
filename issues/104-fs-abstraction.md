# 104 — Filesystem Abstraction

## Current behavior

Partly built (2026-11-04, for the asset resolver,
[105a](./105a-loose-overlays-and-asset-resolver.md)): the case-folded
folder index, `src/059-loose-folders.c`, in the reader library.
`fs_index_folder` walks a folder once (through links, 32 deep at most)
and lists every file under its name in small letters with `/`;
`fs_index_find` finds a file in any spelling (a pack's
`Creature/Wolf/Wolf.M2` by `creature\wolf\wolf.m2`, checked in the
test program); two names differing only by capitals fail to index,
naming both (checked); a folder that isn't there is `W_ERR_NOT_FOUND`.
`fs_normalise` folds a name. Errors are `WErr` with a sentence
(013-errors.h), never `errno`.

Not built: everything else below. Callers still `fopen` files
themselves; there is no `fs_read_all`, no stream, no `fs_join` and the
rest, no allocator hook, and the asset roots come from 105a's settings
(`assetPacks`, `assetOverlays`) and `scripts/run`'s arguments, not
from this layer.

## Intended behavior

A small `fs_*` API, part of the shared reader library (`libwreaders.so`, see
[101](./101-build-system.md)), that:

- Resolves logical asset paths against the configured roots: the Blizzard
  asset root, the replacement pack folders and the overlay folders
  ([103](./103-config-loader.md)).
- Matches paths **case-insensitively from the start**. Blizzard's paths use
  backslashes and mixed case (`World\Maps\Azeroth\Azeroth_32_48.adt`); packs
  and overlays on Linux are case-sensitive folders. The layer normalises to
  lower case with `/` and keeps a per-folder index built at open time, so a
  pack author's `Wolf.M2` and a request for `wolf.m2` meet. Two files in one
  folder that differ only by case are an error at index time, not a silent
  pick.
- Returns typed errors (`err_t`), never `errno`.
- Provides both a "load whole file into a buffer" path and an "open a stream"
  path (for MPQ, which prefers to seek over a large archive).
- Has a single allocator hook so future arena work doesn't touch every call site.

```c
err_t fs_read_all(const char *logical_path, buf_t *out);
err_t fs_open    (const char *logical_path, fs_stream_t *out);
void  fs_close   (fs_stream_t *);
err_t fs_index_folder(const char *root, fs_index_t **out);   /* case-folded */
bool  fs_index_find  (const fs_index_t *, const char *logical, const char **real_path);
```

The [MPQ chain](./105-mpq-reader.md) and the
[asset resolver](./105a-loose-overlays-and-asset-resolver.md) sit on top of
this; the [DBC](./106-dbc-parser.md) and [BLP](./107-blp-parser.md) parsers
read through them.

## Suggested implementation steps

1. Create `src/fs/` subsystem, built into both the executable and `libwreaders.so`.
2. Define `buf_t` (pointer + size + capacity + owning allocator handle).
3. Implement `fs_read_all` over `fopen`/`fread`. Map errno into `err_t`.
4. Implement `fs_open` returning a `FILE*`-wrapping stream with seek and read.
5. Implement the case-folded folder index (walk once at open; map lower-cased
   logical path → real path; duplicate-by-case is an error).
6. Add `fs_join`, `fs_basename`, `fs_extension`, `fs_normalise` (backslash → `/`, lower case).
7. Tests over fixture folders, including a duplicate-by-case fixture.

## Acceptance

- `data/lichking.mpq` resolves to `<asset_root>/Data/lichking.MPQ`.
- A pack file named `Creature/Wolf/Wolf.M2` is found by `creature\wolf\wolf.m2`.
- Reading a missing file returns `ERR_IO` with a log line that says what.
- A folder with two names differing only by case fails to index, naming both.

## Related

- Blocked by: [103](./103-config-loader.md).
- Blocks: [105](./105-mpq-reader.md), [105a](./105a-loose-overlays-and-asset-resolver.md),
  [106](./106-dbc-parser.md), [107](./107-blp-parser.md).
