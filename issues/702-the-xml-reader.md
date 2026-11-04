# 702 — The XML Reader: Templates, Virtual Frames and Errors

*Written 2026-10-16 from the clean-room checklist's note 1.10
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 7's capstone sketched for "XML pull-parser (FrameXML
subset)". [docs/006](../docs/006-ui-and-addons.md) already settles the
reader itself: a pull-parser of the client's own, no libxml2.*

## Blocked by

- Nothing for the reading, built 2026-11-04. Running the scripts its
  files hold waits for [701](./701-the-lua-environment.md); acting on the
  taint it keeps waits for [709](./709-taint-and-combat-lockdown.md).

## Blocks

- [703](./703-frames-anchors-and-layers.md), [706](./706-the-addon-loader.md),
  [708](./708-phase-demo.md).

## Current behavior

Built 2026-11-04 (branch `claude/framexml-reader`, not yet merged), as
background work the owner asked for: everything here that needs neither
Lua nor drawing.

- `src/065-xml.c` / `.h`: the pull-parser (step 1). Hands over each
  opening tag (name, attributes, line, column, depth), run of text and
  closing tag in order. Reads the part of XML 1.0 FrameXML uses:
  comments, CDATA, the five named entities and numbered ones (as
  UTF-8), a byte-order mark, `<?xml?>` and other `<? ?>`, a DOCTYPE with
  no declarations of its own; `xmlns:xsi` and the like as ordinary
  attributes. Refuses what isn't well-formed with W_ERR_CORRUPT and a
  sentence giving the line and column (`line 4, column 1: </Ui> closes
  <Frame>, which opened on line 2`); a DOCTYPE with declarations and a
  UTF-16 file are W_ERR_UNSUPPORTED; nesting past 1024 is refused.
- `src/066-framexml.c` / `.h`: the element tree and what each file makes
  (steps 2 to 4), behind an `Fxml` handle:
  - `fxml_load` reads a file whole first, so a file that isn't
    well-formed (or whose top element isn't `<Ui>`) is skipped whole and
    keeps nothing; then does its top-level elements in order. Templates
    are kept by name, the first definition winning except that a
    trusted file's replaces an addon's; frames, regions and fonts are
    made (`FxmlObject`); `<Script>` becomes a step for the Lua
    environment; `<Include file>` is opened through the caller's `open`
    and done in place, with the includer's taint.
  - An object: the templates applied in this issue's order (`pieces`),
    its attributes with the last written winning (`sets`), its other
    child elements in load order for 703 (`parts`: `<Size>`,
    `<Backdrop>` ...), its parent (the quirk below and its correction
    switch, `FxmlOptions.parent_from_any_template`), its name with
    `$parent` worked out, its anchors with their defaults and
    warnings, setAllPoints, its handlers (`<OnLoad>` text from plain
    text or CDATA, `function=`), its children in load order (regions
    with their layer, named slots such as `NormalTexture`). Every
    element keeps its file, and every file its taint ("" for trusted
    code, else the addon's name), so each piece says whose code
    defined it.
  - `fxml_create`: what CreateFrame does with a template (705), the same
    making with the parent named.
  - The interface log: each warning with its kind, file and line, given
    to the caller's `warn` as it happens and kept;
    `fxml_warning_line` writes `file:line sentence`. The wording table
    is one place in `066-framexml.c`.
  - Kept as data, not run: every script's text, its line, and the
    taint of the code that defined it. Running them is 701's; acting on
    the taint, 709's.
- `scripts/build`: both join the reader library (READERS), so
  world-edit-to-execute can read interface XML through LuaJIT's FFI
  (checked: LuaJIT loads `libwreaders.so` with the flattened header and
  reads a template through it).

Checked:
- `tests/044-test-client.c`, "the interface's XML", before the archives
  (so it runs with no game data), on files the test writes: a default
  interface file of templates and an addon file using them, the addon
  including a file with a missing closing tag and a good one, a
  `Blizzard_` addon loaded after it. 52 checks, each step 5 item among
  them: `inherits="A, B"` (B's `<Size>` the last written, A's children
  before B's, A's own template first); a loop through two templates
  warning once while the frame still loads; a missing template; a second
  addon template ignored and a trusted one replacing it (frames made
  before keep theirs); `$parentButton` under an unnamed frame under
  `Foo` is `FooButton`; the broken file skipped and the next loading;
  the parent quirk with the correction off and on. Also: the reader's
  pieces and 19 broken pieces of XML each refused with its sentence;
  nesting at and past the limit; anchors' defaults, offsets and three
  kinds of bad anchor; setAllPoints against anchors; scripts from CDATA
  and plain text with entities and their taint; steps in order;
  CreateFrame with a template; templates that would double forty times
  stopped at the limit in about 12 ms; 3000 damaged copies of the files
  each read or refused with a sentence. 2026-11-04: all 52 pass, alone
  (scripts/build's flags) and in the whole test program (built with
  stand-in raylib headers, as raylib isn't installed here; the earlier
  checks fail there for want of game data, as they always do without
  it); the same under the address and undefined-behaviour checkers
  (gcc), whose only leak reports are the test program's own from
  stopping at the archives. A separate fuzz run under the same checkers
  (about 2.1 million generated and damaged documents through both
  parts, 2026-11-04) found nothing.

## Intended behavior

The default interface's XML and every addon's XML build the same frames
they build in the original, and a broken file breaks only itself.

- **Virtual frames** (note 1.10): a named top-level `virtual="true"`
  element is kept as a template, not created; an unnamed one is a
  warning. The first definition of a name wins, except that a trusted
  (default interface) definition replaces an addon's.
- **Inheriting:** `inherits="A, B"` applies A fully (its own inherits
  first), then B, then the element's own attributes and children; the
  last value written wins; children load in that same order. Each
  template's content loads with the taint of the code that defined it
  ([709](./709-taint-and-combat-lockdown.md)). Missing and self-inheriting
  templates are warnings, skipped at that point.
- **Parent:** the `parent` attribute, else found through the templates,
  where the **last** template listed decides (a **quirk**, below); an
  unknown parent is a warning and the frame has none.
- **`$parent`** at the start of a name, any case: the nearest named
  ancestor's name, or `Top`.
- **Anchors in XML:** defaults for a missing `relativeTo` and
  `relativePoint`; bad points, unknown frames and self-anchoring are
  warnings and that anchor is skipped; `setAllPoints` with anchors is a
  warning and ignored.
- **Errors:** nearly everything is a warning written to the interface
  log, and loading carries on. A file that can't be opened or isn't
  well-formed is skipped whole; an `Include` with no `file` is skipped.

### Quirks, each with a correction switch (off by default: the original)

| Quirk (docs/057) | The original | The correction |
|---|---|---|
| parent from templates (1.10) | only the last template listed (and what it inherits) can give the parent; an earlier template's `parent` is ignored | take the parent from the last template listed that has one |

The XML frame-level quirk (a `frameLevel` of 0 refused) is read here but
listed with its switch in [703](./703-frames-anchors-and-layers.md).

## Suggested implementation steps

1. ~~The pull-parser (docs/006): elements, attributes, text, with line
   numbers for warnings.~~ Built (065).
2. ~~The template table, with the trusted-replaces-addon rule.~~ Built (066).
3. ~~Building a frame: templates in order, then itself; parent finding;
   `$parent`; anchors.~~ Built (066).
4. ~~The interface log: every warning with file and line, in the
   original's wording.~~ Built, with the wording still to check (Open, 1).
5. ~~Tests, each answering note 1.10~~ (all six in the test program):
   - `inherits="A, B"` where both set the width: B's width wins, and
     A's children load before B's;
   - a template inheriting itself through another warns once and the
     rest of the frame still loads;
   - a second addon template of the same name is ignored; a default
     interface one replaces it;
   - `$parentButton` under an unnamed frame under `Foo` is `FooButton`;
   - a file with a missing closing tag is skipped and the next file
     loads;
   - the parent quirk with the correction off and on.
6. Running the scripts the steps and handlers hold, where they stand
   (701), each with the taint 702 kept for it (709).
7. Checking elements and attributes against the schema the archives
   ship (`Interface\FrameXML\UI.xsd`), so unknown ones are warned about
   as the original does, once the archives are at hand.

## Decisions

Made 2026-11-04 without the owner (background work); each is easy to
turn round.

- **Note 1.10 isn't in this copy of docs/057.** The row ("XML templates
  inherit by these rules ...") is there with its answer empty, and no
  numbered note 1.10 follows section 1, so the rules were built from this
  issue's own text. Where the issue is silent, the choices below.
- **Two files, both in the reader library.** 065 is a plain XML reader,
  066 the interface's meaning on top. Both are window-free, read files
  the archives and addons hold, and need nothing from the settings
  store, so they join READERS (scripts/build) rather than only CORE:
  world-edit-to-execute can read and check interface XML through the
  same FFI as the other readers. The Lua environment, by contrast, goes
  to CORE (701).
- **The correction switch is an option on the handle**, not yet a
  setting: 066 stays free of the settings store so it can sit in the
  library. The addon loader (706), the first part that reads the
  player's interface settings, declares it in `014-settings.c` beside
  703's switches.
- **The log's wording.** Three lines are written as public copies of the
  original's `FrameXML.log` show them (`Couldn't find inherited node
  "X"`, `Deferred XML Node object named X already exists`, `Unrecognized
  XML: X`), unchecked against the note; the rest are this client's own.
  All of them are one table in `066-framexml.c`, marked line by line.
- **Lenient where XML 1.0 is strict without the meaning changing** (a
  `--` inside a comment, `]]>` in text, control bytes other than zero),
  strict on structure (tags, quotes, entities, a `<` in a value), since
  an addon the original loads must load here and a broken one must say
  where.
- **Names and case.** Template names are matched ignoring case;
  element names (`<frame>`), attribute names (`setallpoints`), anchor
  points and the `true` of `virtual` and `setAllPoints` too; frame names
  (Lua globals, `relativeTo`, `parent`) exactly.
- **Parents.** A frame inside another takes it as its parent unless it
  (or, by the quirk, its last template) names one. In the original's
  quirk the last template listed counts whether or not it exists.
  `$parent` follows the parent chain as found (so `parent="UIParent"`
  makes `$parentX` into `UIParentX`), and is `Top` with none.
- **Order of making.** A frame's parent and name are found when its file
  reaches it, then its anchors (against frames made by then: those
  above it, before it, and itself), then its children; a template is
  found by frames made after it is defined, not before. A second anchor
  to the same point replaces the first; the last handler of each name
  wins, while `pieces` keeps every template applied.
- **Smaller calls.** An unnamed virtual element is skipped; `virtual`
  on an element inside another is a warning and the element is made as
  an ordinary one; a top-level element that is neither made nor run
  (`<Size>` at the top) is a warning; a file whose top element isn't
  `<Ui>` is skipped whole; a trusted template replacing an addon's says
  nothing in the log; frames made from the addon's earlier keep it.
- **Limits, each a warning, then carrying on:** 1024 elements nested
  (the reader), 256 objects inside objects, 64 templates inheriting
  templates, 100000 objects made or templates applied for one top-level
  element (templates listing one twice would otherwise double each
  level), 32 includes deep.
- **Paths** in `<Script file>` and `<Include file>` are joined to the
  XML file's folder with the archives' backslashes.
- **Sanitizers with gcc.** clang here has no address-checker runtime.

## Open

1. **The wording** of every log line against note 1.10 when the
   describer fills it (one table to change).
2. **The order of making against the original's**: when a frame's
   anchors are resolved against its own children, and whether the
   original reads a frame's child elements in a fixed order rather than
   as written. docs/057 rows for section 1.
3. **Two templates' children with the same name** (the issue's own open
   point, waiting on docs/057): both are made today, the later one
   holding the name.
4. **UI.xsd** (step 7), and the frame types listed in `066-framexml.c`,
   checked against it on the owner's machine.

## Related

- [docs/006](../docs/006-ui-and-addons.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 1.10,
  [701](./701-the-lua-environment.md),
  [703](./703-frames-anchors-and-layers.md),
  [706](./706-the-addon-loader.md),
  [709](./709-taint-and-combat-lockdown.md).
