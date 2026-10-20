# 702 — The XML Reader: Templates, Virtual Frames and Errors

*Written 2026-10-16 from the clean-room checklist's note 1.10
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 7's capstone sketched for "XML pull-parser (FrameXML
subset)". [docs/006](../docs/006-ui-and-addons.md) already settles the
reader itself: a pull-parser of the client's own, no libxml2.*

## Blocks

- [703](./703-frames-anchors-and-layers.md), [708](./708-phase-demo.md).

## Current behavior

Nothing exists. No XML is read; the default interface's files sit unread
in the archives.

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

1. The pull-parser (docs/006): elements, attributes, text, with line
   numbers for warnings.
2. The template table, with the trusted-replaces-addon rule.
3. Building a frame: templates in order, then itself; parent finding;
   `$parent`; anchors.
4. The interface log: every warning with file and line, in the
   original's wording.
5. Tests, each answering note 1.10:
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

## Open questions

None. What happens when two templates' children carry the same name
waits on docs/057.

## Related

- [docs/006](../docs/006-ui-and-addons.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 1.10,
  [703](./703-frames-anchors-and-layers.md),
  [709](./709-taint-and-combat-lockdown.md).
