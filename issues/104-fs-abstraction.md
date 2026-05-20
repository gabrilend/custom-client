# 104 — Filesystem Abstraction

## Current behavior

There is no abstraction over file I/O. Every caller would reach for
`fopen` directly, scatter `errno` checks across the codebase, and have no
shared notion of where the asset root lives.

## Intended behavior

A small `fs_*` API that:

- Resolves logical asset paths against the configured asset root.
- Returns typed errors (`err_t`), never `errno`.
- Provides both a "load whole file into a buffer" path (for parsing
  modest-sized files) and an "open a stream" path (for MPQ — which prefers
  to seek over a large archive).
- Has a single allocator hook so future arena/stack-allocator work doesn't
  require touching every call site.

```c
err_t fs_read_all(const char *logical_path, buf_t *out);
err_t fs_open    (const char *logical_path, fs_stream_t *out);
void  fs_close   (fs_stream_t *);
```

This is the layer that the [MPQ reader](./105-mpq-reader.md) will sit on
top of when reading the archives themselves, and the [DBC](./106-dbc-parser.md)
and [BLP](./107-blp-parser.md) parsers will sit on top of when reading
files out of those archives.

## Suggested implementation steps

1. Create `src/fs/` subsystem.
2. Define `buf_t` (pointer + size + capacity + owning allocator handle).
3. Implement `fs_read_all` over `fopen`/`fread`. Map errno into `err_t`.
4. Implement `fs_open` returning a `FILE*`-wrapping stream with seek and
   read. Skip mmap for v1 — premature.
5. Add `fs_join`, `fs_basename`, `fs_extension` path helpers. Cross-platform
   path separators are normalized to `/` internally; the OS layer translates
   on Windows.
6. Tests over fixture files.

## Acceptance

- A caller can request `data/lichking.mpq` and have it resolve to
  `<asset_root>/Data/lichking.MPQ` (case-insensitive lookup is a future
  issue — for now we honor the case in the config).
- Reading a missing file returns `ERR_IO` with a log line that says what.

## Related

- Blocked by: [103](./103-config-loader.md).
- Blocks: [105](./105-mpq-reader.md), [106](./106-dbc-parser.md),
  [107](./107-blp-parser.md).
