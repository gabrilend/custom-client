# 509 — The Cache Files (WDB)

*Written 2026-10-16 from the clean-room checklist's note 10.1
([docs/057](../docs/057-what-the-original-program-decides.md)). The
server answers questions about creatures, items, quests and names one
record at a time; the original keeps the answers on disk between runs.*

## Blocked by

- The world connection and its packet codec (sketched in Phase 5's
  capstone, not yet written as issues).

## Blocks

- [508](./508-phase-demo.md), [813](./813-auto-loot.md).

## Current behavior

Nothing exists. There's no connection, so nothing is asked for or kept.
The world's creatures come from a list generated from a world database
([404](./404-characters-and-equipment.md)) until the server sends them.

## Intended behavior

The client keeps the same caches, in the same files, in the same shape,
and asks the server no more often than the original does, so a server
sees the same load and the original's own cache folder could be read.

- **Where** (note 10.1): `Cache/WDB/<locale>/` when the game's data is
  present, otherwise `WDB/`; folders made.
- **Which:** fifteen kinds, each with a four-letter signature; nine kept
  on disk (creatures, game objects, item names, items, NPC text, quests,
  page text, item text, anti-cheat modules), six in memory only.
- **The file:** a header of six little-endian 32-bit numbers (signature,
  build 12340, locale, record size, format version, cache version), then
  entries of key, length and packed record, ended by a zero key with
  zero length.
- **Thrown away** when any of the first five header numbers differs, or
  everything from a file that stops mid-entry.
- **Refresh:** a new cache version from the server throws all entries
  out; a missing record is asked for and stored when answered; some
  records are memory-only.
- **Rate limits:** at most 256 item queries and 128 name queries in each
  30-second window, the rest queued in order for the next window; other
  kinds unlimited.
- **Written** in full when each cache shuts down.
- **What's inside a record:** the record part of the server's reply to
  that cache's query, stored as received, fields in the reply's order,
  text as zero-terminated strings; the writer puts them back the same
  way. Format versions: item 5, quest 3, name 4, the rest 1. A reply id
  with its top bit set (2147483648 added) means "no such record":
  everyone waiting is told it failed and nothing is stored. Replies
  with several records start with a one-byte count.

## Suggested implementation steps

1. The file reader and writer, header checks, the damaged-file rule.
2. One cache per kind: lookup, ask-on-miss, store-on-answer, the
   memory-only kinds.
3. The two query limits with their queue.
4. Writing on shutdown.
5. Tests:
   - a file written and read back gives the same entries;
   - a file with build 12341 in its header is ignored whole;
   - a file cut mid-entry loads nothing;
   - 300 item queries in one moment send 256 and the other 44 thirty
     seconds later;
   - a changed cache version from the server empties the caches.

## What is built (2026-11-05)

`src/104-wdb.c/.h`, with no connection yet: the file, one cache per
kind, the two limits and the shutdown write. The caller sends the
queries (`WdbAsk`) and hands each answer in (`wdb_answer`); the world
connection will be that caller. Thirty-nine checks in
`tests/044-test-client.c` (`test_wdb`), on files the test writes, so
they need no game data: the five tests step 5 names, each header number
in turn, cuts at every seventh byte, waiters told in order, no such
record, the window's edges, and the memory-only kinds. They pass under
gcc's address and undefined-behaviour checkers. Mutation check: 32
mutants, all caught.

Still open: the kinds' own values (step 2's table) and the queries'
wiring, which wait on docs/057's note 10.1 and the world connection;
the goodbye receipt's line for the caches; and splitting a reply with
several records, which belongs to the packet code.

## Decisions (2026-11-05)

Made without the owner, each one theirs to overturn.

1. **The kinds are the caller's, not a table here yet.** The issue
   names the nine kinds kept on disk and three format versions, but not
   the signatures, the record sizes or the six memory-only kinds; those
   are note 10.1's, which this copy of docs/057 doesn't carry. A
   `WdbKind` holds them, and the numbers the issue states are constants
   (`WDB_BUILD`, `WDB_FORMAT_*`, `WDB_ITEM_LIMIT`, `WDB_NAME_LIMIT`).
2. **A kind with no signature yet is kept in memory only**, even if it
   names its file, and one kept on disk with no record size is refused.
   So no file is written that the original could misread.
3. **The cache version is not one of the checked numbers.** A file
   whose sixth number differs still loads; the server's own version
   (`wdb_server_version`) empties the cache when it differs. This
   follows the issue's "first five" and its refresh rule.
4. **"Stops mid-entry" includes a missing end.** A file that runs out
   between entries, without the zero key and zero length, loads nothing.
   So does an entry longer than the file has left. Bytes after the end
   are not read.
5. **A window starts with its first query and lasts 30 seconds.** The
   queue goes first when a window opens, in the order wanted, through
   `wdb_tick` or the next `wdb_want`. A record wanted twice is asked for
   once, and every waiter is told, in the order they wanted.
6. **A reply with several records is split before the cache.** The
   cache takes one record an answer, because only the packet code knows
   each kind's layout, which the one-byte count is followed by.
7. **Written in key order, through a file beside it.** The same cache
   writes the same bytes, and a run stopped mid-write leaves the old
   file. The folder is made where missing, its root included, but not
   the root's parents.
8. **In the core, not the reader library.** The cache is the client's
   state between runs, not a game file world-edit-to-execute reads, so
   it joins CORE in `scripts/build`, not READERS.

## Open questions

None. Which records are memory-only, and the record sizes the header
carries, wait on docs/057. The record sizes are the original's own
in-memory sizes, so a file the original itself must accept needs them
written as plain numbers.

## Related

- [docs/004](../docs/004-network-protocol.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 10.1.
