# 105 — MPQ Reader

## Current behavior

The client cannot read MPQ archives, which means it cannot read any of
the original game's assets. Everything downstream is blocked.

## Intended behavior

A thin C wrapper around StormLib that exposes:

```c
err_t mpq_chain_open (const char **archive_paths, int n, mpq_chain_t **out);
void  mpq_chain_close(mpq_chain_t *);

err_t mpq_chain_read (mpq_chain_t *, const char *logical, buf_t *out);
bool  mpq_chain_has  (mpq_chain_t *, const char *logical);
```

`mpq_chain_t` owns a list of opened archives in **patch-chain order**.
Reads walk the chain back-to-front so that later patch archives
correctly override earlier ones — this is the same semantics the original
client uses, and is the only way the live `patch-3.MPQ` files get
applied.

Default chain (loaded from `client.ini`):

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
2. Create `src/mpq/` subsystem.
3. Implement open/close, wrapping `SFileOpenArchive`.
4. Implement the chain walk for read + exists.
5. Implement a small directory iterator (`SFileFindFirstFile` /
   `SFileFindNextFile`) — needed by the [DBC](./106-dbc-parser.md) issue
   for "list all DBCs in DBFilesClient/".
6. Tests against a fixture chain (small synthetic MPQs we generate in the
   test setup; do NOT commit Blizzard MPQs).

## Acceptance

- Opening a real Blizzard MPQ chain succeeds and reads a known file
  (`DBFilesClient/Map.dbc`) that is byte-identical to what StormLib's
  command-line tool extracts.
- A file present only in `patch-3.MPQ` resolves correctly.
- Closing the chain frees all StormLib handles (verified under ASan).

## Stress markers

The patch-chain ordering is the part with subtle correctness consequences.
Get it wrong and you ship a client that's silently running pre-3.3.5
data for some files. A test that asserts on a known-overridden file is
worth its weight.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#container-mpq)
- Blocked by: [104](./104-fs-abstraction.md).
- Blocks: [106](./106-dbc-parser.md), [107](./107-blp-parser.md), every
  asset-format issue in later phases.
