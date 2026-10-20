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

## Open questions

None. Which records are memory-only, and the record sizes the header
carries, wait on docs/057. The record sizes are the original's own
in-memory sizes, so a file the original itself must accept needs them
written as plain numbers.

## Related

- [docs/004](../docs/004-network-protocol.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 10.1.
