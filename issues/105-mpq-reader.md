# 105 — MPQ Chain (Read and Write)

## Current behavior

The client cannot read MPQ archives, which means it cannot read any of the
original game's assets. world-edit-to-execute has its own Lua MPQ reader for
WC3 maps, but it lacks bzip2 decompression and the 44-byte header that WoW's
archives use, and it has no idea of a chain of archives.

## Intended behavior

A C wrapper around StormLib, part of the shared reader library
(`libwreaders.so`), that exposes:

```c
err_t mpq_chain_open (const char **archive_paths, int n, mpq_chain_t **out);
void  mpq_chain_close(mpq_chain_t *);

err_t mpq_chain_read (mpq_chain_t *, const char *logical, buf_t *out);
bool  mpq_chain_has  (mpq_chain_t *, const char *logical);
err_t mpq_chain_which(mpq_chain_t *, const char *logical, const char **archive);
err_t mpq_chain_list (mpq_chain_t *, const char *pattern, strlist_t *out);

err_t mpq_write      (const char *out_path, const mpq_entry_t *entries, int n);
```

- `mpq_chain_t` owns the opened archives in **patch-chain order**. Reads walk
  it back to front so later archives override earlier ones, the same rule as
  the original client.
- `which` names the archive that supplied a file. It is used by the demos
  (override counts), by the asset resolver's source report, and by
  world-edit-to-execute when it checks what its patch archive overrides.
- `list` enumerates files matching a pattern across the chain (DBC listing,
  model catalogues for world-edit-to-execute's W05b).
- A read that finds nothing returns `ERR_NOT_FOUND` and logs the path and the
  archives searched.
- `mpq_write` builds an archive from a list of (logical path, bytes). It is
  used by world-edit-to-execute to build `patch-W.MPQ` for the **stock**
  client, and by the "default client compatible" seal check. The W client
  itself does not need patch archives for converted maps: it reads them as
  loose overlays ([105a](./105a-loose-overlays-and-asset-resolver.md)).

Default chain (from `client.ini`), then any `[paths] extra_archives`:

```
Data/common.MPQ
Data/common-2.MPQ
Data/expansion.MPQ
Data/lichking.MPQ
Data/{locale}/locale-{locale}.MPQ
Data/{locale}/expansion-locale-{locale}.MPQ
Data/{locale}/lichking-locale-{locale}.MPQ
Data/patch.MPQ
Data/patch-2.MPQ
Data/patch-3.MPQ
Data/{locale}/patch-{locale}.MPQ
Data/{locale}/patch-{locale}-2.MPQ
Data/{locale}/patch-{locale}-3.MPQ
```

## Suggested implementation steps

1. Vendor StormLib at `libs/stormlib`. Add as a CMake subdirectory.
2. Create `src/mpq/` subsystem, built into both targets.
3. Open/close wrapping `SFileOpenArchive`.
4. Chain walk for `read`, `has`, `which`.
5. `list` via `SFileFindFirstFile` / `SFileFindNextFile` across the chain,
   de-duplicated by logical path (the winning archive is reported).
6. `mpq_write` via `SFileCreateArchive` + `SFileAddFileEx` (zlib compression,
   listfile included so tools can browse the result).
7. An FFI header (`wreaders_ffi.h`, plain C declarations only) that
   world-edit-to-execute feeds to `ffi.cdef`.
8. Tests against a fixture chain of small synthetic MPQs made by `mpq_write`
   in test setup (never commit Blizzard MPQs): override order, `which`,
   `list`, round trip write → read.

## Acceptance

- Opening a real Blizzard chain succeeds and reads `DBFilesClient/Map.dbc`
  byte-identical to StormLib's command-line extraction.
- A file present only in `patch-3.MPQ` resolves, and `which` says `patch-3.MPQ`.
- An archive written by `mpq_write` opens in the stock client's chain
  (checked once by hand, then by world-edit-to-execute's W04 rig).
- world-edit-to-execute reads `Map.dbc` through the FFI from LuaJIT.
- Closing the chain frees all StormLib handles (ASan clean).

## Stress markers

The patch-chain ordering is the part with subtle correctness consequences.
Get it wrong and you ship a client that's silently running pre-3.3.5 data for
some files. A test that asserts on a known-overridden file is worth its weight.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#container-mpq)
- world-edit-to-execute `issues/W01-read-the-wow-client-archives.md` (now implemented here)
- Blocked by: [104](./104-fs-abstraction.md).
- Blocks: [105a](./105a-loose-overlays-and-asset-resolver.md), [106](./106-dbc-parser.md),
  [107](./107-blp-parser.md), every asset-format issue in later phases.
