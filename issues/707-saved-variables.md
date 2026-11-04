# 707 — Saved Variables

*Written 2026-10-16 from the clean-room checklist's note 1.6
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 7's capstone sketched for "saved variables (load on
startup, write on logout)".*

## Blocked by

- [704](./704-events-and-the-per-frame-update.md) (`PLAYER_LOGOUT` and
  the other shutdown events), [706](./706-the-addon-loader.md) (the
  names each addon saves, from its `.toc`).

## Blocks

- [708](./708-phase-demo.md) ("persists a saved variable across a
  restart"), [609](./609-losing-the-connection.md).

## Current behavior

Nothing exists. Addons can't be loaded, so nothing is saved.

## Intended behavior

An addon's saved file written by this client reads back in the original,
and the reverse, and both are written at the same moments.

- **Where** (note 1.6): `WTF\Account\<account>\SavedVariables\<addon>.lua`
  for account-wide, `<realm>\<character>\SavedVariables\<addon>.lua` per
  character, `SavedVariables.lua` for the default interface's own;
  missing folders made.
- **When:** only at interface shutdown (logout, reload, losing the
  connection); never on a crash.
- **The shutdown order:** combat lockdown lifted with its event, leave
  the world, `PLAYER_LOGOUT`, then the program's own saves in their
  order, the default interface's variables, moved frames' positions,
  settings, and last each addon's files and which addons are on.
- **Backups:** the old file renamed to `.bak` first; a file deleted when
  the addon no longer declares variables of that kind.
- **Which names:** only letters, digits, `_` and bytes above 127; others
  skipped silently.
- **The text format:** a blank line before each `Name = value`; CR LF
  line ends; numbers at 16 significant digits in the general style;
  `nil --[[ nan ]]` and `nil --[[ inf ]]`; text in double quotes with
  `\"`, `\\`, `\n`, `\r`, `\000` and every other byte as it is; tables
  with one tab per level, the list part first as `value, -- [i]`, then
  the rest as `[key] = value,`; functions and other kinds as `nil` with a
  comment; a table inside itself as `nil --[[ skipped recursive table ]]`;
  a table merely shared written in full each time.
- **Loading:** the file runs as a script when the addon loads; an error
  is reported and loading carries on.

## Suggested implementation steps

1. The writer, one value kind at a time, to the format above.
2. The shutdown order, run on logout, reload and losing the connection
   (609).
3. The backups and the deletion rule; the name filter.
4. Loading as a script, in the addon's environment.
5. Tests, each answering note 1.6:
   - 0.1 writes as `0.1` and 1/3 as `0.3333333333333333`;
   - a table `{10, 20, x = 1}` writes its two list entries with their
     `-- [1]` and `-- [2]` comments, then `["x"] = 1,`;
   - a table that holds itself writes the recursion comment and the file
     still loads;
   - a shared table loads back as two separate tables;
   - a variable named `my-var` is skipped;
   - writing twice leaves the first version as `.bak`;
   - a file written by this client, loaded and written again, is the
     same bytes, apart from the order of non-list keys.

## Open questions

None. Whether quitting the program from the world runs the same
shutdown waits on docs/057.

## Related

- [docs/006](../docs/006-ui-and-addons.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 1.6,
  [704](./704-events-and-the-per-frame-update.md),
  [609](./609-losing-the-connection.md).
