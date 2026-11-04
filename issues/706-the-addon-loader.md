# 706 — The Addon Loader

*Written 2026-11-04 from [docs/006](../docs/006-ui-and-addons.md)
("What an addon is", "Loading order") and
[docs/057](../docs/057-what-the-original-program-decides.md) section 1,
at the number Phase 7's capstone sketched for "addon loader (.toc parser,
dependency sort, load)". Background work the owner asked for; not
built.*

## Blocked by

- [701](./701-the-lua-environment.md) (running an addon's Lua),
  [702](./702-the-xml-reader.md) (its XML; built 2026-11-04, branch
  `claude/framexml-reader`), [704](./704-events-and-the-per-frame-update.md)
  (`ADDON_LOADED`).

## Blocks

- [707](./707-saved-variables.md) (the names an addon saves are listed in
  its `.toc`), [708](./708-phase-demo.md) ("the default UI loads; so does
  a test addon").

## Current behavior

Nothing loads an addon. The default interface's files sit in the
archives and an addon's in its folder; 702 can read any XML file it is
handed, with the taint it is told.

## Intended behavior

The default interface and every enabled addon load in the original's
order, from the same files, with the same results for addons that
depend on one another, so an addon written for the original loads here
unchanged (docs/006).

- **What an addon is** (docs/006): a folder `Interface\AddOns\<Name>`
  holding `<Name>.toc`: `## Key: Value` lines (`Interface`, `Title`
  and its localized `Title-xxXX`, `Notes`, `Author`, `Version`,
  `Dependencies` / `RequiredDeps` / `Dep...`, `OptionalDeps`,
  `SavedVariables`, `SavedVariablesPerCharacter`, `LoadOnDemand`,
  `LoadWith`, `LoadManagers`, `DefaultState`, and any `X-` key an addon
  invents, kept for `GetAddOnMetadata`), `#` comments, and its files,
  one a line, in load order (`.lua` and `.xml`, paths from the folder,
  either slash).
- **Where they are:** the default interface (`Interface\FrameXML\FrameXML.toc`)
  and the signed `Blizzard_` addons in the archives (015); every other
  addon in the profile's own `Interface\AddOns` folder on disk (docs/001,
  "Profiles"). Whether a folder there named as an archive's addon
  replaces it is the original's to say (docs/057); until then it
  doesn't, since the archive's is trusted code.
- **Trust:** the default interface and the `Blizzard_` addons from the
  archives are trusted code; every other addon's files load with its
  own name as their taint (702 keeps it on every frame and script;
  [709](./709-taint-and-combat-lockdown.md) acts on it).
- **Which load:** each addon's on/off state per character, in the
  original's `AddOns.txt` (`Name: enabled` or `disabled`, beside the
  character's saved variables); an addon whose `Interface` number is
  older than the client's (30300) is "out of date" and not loaded unless
  the player allows out-of-date addons, as the original's addon list
  does.
- **Order** (docs/006, "Loading order"): the default interface first;
  then the enabled addons, each after everything it lists in
  `Dependencies` and `OptionalDeps` (a sort over both, which only
  `Dependencies` can stop). A cycle is a typed error naming the addons
  in it, and they are disabled (docs/006); a missing or disabled
  required dependency stops the addon, with the reason the original's
  interface shows (its `ADDON_DEP_MISSING`, `ADDON_DEP_DISABLED`,
  `ADDON_INTERFACE_VERSION`, ... strings, which are the shipped
  interface's data). Among addons nothing orders, the order is the
  original's (to be checked; until then, by name, ignoring case).
- **One addon, loaded:** its files in the `.toc`'s order: a `.lua` file
  run in the addon's environment (701); an `.xml` file read by 702
  (`fxml_load`, with the addon's taint and a way to open its includes),
  its steps then run in order (701); a missing or broken file written to
  the interface log, and the rest of the addon still loads. Then its
  saved variables load (707), then `ADDON_LOADED` fires with its name
  (docs/006 step 4), so its handler sees its saved values.
- **Later loads:** `LoadOnDemand` addons load when `LoadAddOn` asks
  (705), or with the addon named in their `LoadWith`; a trusted
  `Blizzard_` addon loaded this way replaces addon templates of the same
  name (702's rule).
- **The interface log** holds, per file, `Loading` lines and 702's
  warnings with file and line, in the original's own log's shape
  (docs/057 note 1.10 for the wording); a line per addon not loaded and
  why.
- **The correction switches**: this is the first part that reads the
  player's settings for the interface, so the settings store
  (`src/014-settings.c`) gets the XML reader's switch (702, "parent from
  templates") declared here, beside 703's.

## Suggested implementation steps

1. The `.toc` reader (window-free, its own file): metadata, files,
   comments, the localized keys; refusals with line numbers.
2. Finding addons: the archives' and the profile's folders; `AddOns.txt`.
3. The order: the dependency sort, cycles, missing and disabled
   dependencies, out-of-date addons.
4. Loading one addon: its files through 701 and 702, its saved
   variables (707), `ADDON_LOADED` (704).
5. Load on demand and `LoadWith`.
6. The interface log; the correction switches declared.
7. Tests, on addons the test program writes:
   - B lists A in `Dependencies`: A loads first, whatever their names;
   - A and B depend on each other: neither loads, and the error names
     both;
   - C needs a missing D: C doesn't load, with the dependency-missing
     reason;
   - an addon file with a broken XML file still loads its other files;
   - `ADDON_LOADED` fires once per addon, after its files, with its
     name;
   - an out-of-date addon loads only with out-of-date addons allowed.

## Acceptance

- The default interface's own `FrameXML.toc` loads from the archives
  with no warning the original's log doesn't also show.
- A test addon (708's) loads after it and prints on `PLAYER_LOGIN`.

## Open questions

None of the owner's. For docs/057 (section 1, new rows): the order
among addons nothing orders; how long a `.toc` line may be before it is
cut; whether a `.toc` file name differing from its folder's in case
loads.

## Related

- [docs/006](../docs/006-ui-and-addons.md), [701](./701-the-lua-environment.md),
  [702](./702-the-xml-reader.md), [707](./707-saved-variables.md),
  [709](./709-taint-and-combat-lockdown.md).
