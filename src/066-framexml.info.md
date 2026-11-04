# 066-framexml

*Written by `scripts/generate-info` from `066-framexml.h`; don't edit, re-run it.*

066-framexml.h (issue 702) - the interface's XML: templates, inheriting, and what each file makes

For a general audience: the default interface and every addon describe
their frames (windows, buttons, bars) in XML files. A frame marked
virtual is a template: it isn't made, but other frames name it in
`inherits` and get its look and its children, as if written into them.
This reads those files (065-xml does the reading), keeps the templates,
and works out, for every frame a file makes, everything the XML says
about it once the templates are applied: its name, its parent, its
attributes, its anchors, its children, its scripts. The frames
themselves (rectangles, drawing, input) are 703's, which builds them
from what this gives; running the scripts is the Lua environment's
(701), which gets their text from here.

The rules are docs/057's note 1.10, as issue 702 gives them:
  - A named top-level element with virtual="true" is a template. The
    first definition of a name wins, except that a trusted one (the
    default interface, signed Blizzard_ addons) replaces an addon's.
  - inherits="A, B" applies A fully (its own inherits first), then B,
    then the element itself; the last value written wins, and children
    load in that same order. A missing template, or one that inherits
    itself (directly or through others), is a warning, skipped there.
  - Parent: the `parent` attribute, else found through the templates,
    where only the last template listed (and what it inherits) can give
    one (a quirk; FxmlOptions.parent_from_any_template corrects it);
    else the frame it is written inside; an unknown parent is a warning
    and the frame has none. Regions and fonts take the frame they are
    written inside.
  - $parent at the start of a name, in any case, is the name of the
    nearest named parent up the chain, or "Top" when there is none.
  - Anchors: relativeTo left out means the parent, relativePoint left
    out means the same point; a bad point, an unknown frame, or a frame
    anchored to itself is a warning and that anchor is skipped; an
    anchor to a point already given replaces it; setAllPoints with
    anchors is a warning and setAllPoints is ignored.
  - Errors: nearly everything is a warning, kept with its file and line
    (the interface log) and loading carries on. A file that can't be
    opened or isn't well-formed is skipped whole: nothing in it is
    kept. An Include with no `file` is skipped.

Trust and taint (709): each file is read with the taint of the code it
belongs to: "" for trusted code, else the addon's name. Every element
keeps its file, so every piece of a frame (each template applied, each
attribute, anchor, child and script) says whose it is. Until 709 exists
nothing acts on it; it is only kept.

Order (decided here; 703 may move it when docs/057 says more): a frame
is made when its file reaches it; its parent and name are found, its
name is known from then on, its anchors are resolved against what is
made by then (frames above it, before it, and itself), and then its
children are made, in order. A frame made before a template is
defined doesn't see it; one made after does.

Which elements are frames, regions and fonts is the fixed list in
066-framexml.c (the widget types the original's program makes). Others
are kept as data (FxmlObject.parts) for 703, unchecked: the original
checks them against the schema the archives ship (Interface\FrameXML\
UI.xsd), and reading that is left to when the archives are at hand.

## What it offers

- `typedef struct FxmlNode FxmlNode;`
- `typedef struct FxmlObject FxmlObject;`
- `type FxmlFile`
  A file the handle has read (its path, whose code it is, its top
  element), and its elements as written.
- `type FxmlAttr`
- `type FxmlKind`
  What an element makes: a frame, a region or a font.
- `type FxmlSet`
  An attribute as it ends up, and the element that wrote it (whose file
  says whose it is).
- `type FxmlAnchor`
- `type FxmlScript`
- `type FxmlStepKind`
  The steps loading took, in order: what the Lua environment (701) does
  next, running each script where its step stands.
- `type FxmlStep`
- `type FxmlWarnKind`
  The interface log: each warning's kind (its wording is in
  066-framexml.c), its file and line, its sentence.
- `type FxmlWarning`
- `type FxmlOptions`
  How a handle reads, field by field (all zero is fine):
    parent_from_any_template  the correction for the quirk "parent from
        templates" (702): 0, the original (only the last template listed
        can give the parent); 1, the last template listed that gives one
    ctx           handed back to the three calls
    frame_exists  is there a frame of this name made outside the XML (by
        the scripts)? NULL: none are
    open          an Include's file: its bytes, from malloc (the handle
        frees them), into *bytes and *len; W_OK, or the failure in why.
        NULL: an Include can't be opened
    warn          each warning as it happens (the interface log). NULL:
        they are only kept
- `typedef struct Fxml Fxml;`
  The interface's XML so far (the handle: docs/001, "Handles"): its
  files, templates, objects, steps and warnings. `opt` is copied; NULL
  is all zero. NULL when there is no memory, with `why` filled.
- `Fxml *fxml_new(const FxmlOptions *opt, WWhy *why);`
- `void fxml_free(Fxml *fx);`
- `WErr fxml_load(Fxml *fx, const char *path, const char *bytes, size_t len, const char *taint, WWhy *why);`
  Reads a file and does everything in it, in order (its templates
  defined, its objects made, its scripts listed as steps, its includes
  opened through FxmlOptions.open and done in place), with `taint` ("" or
  NULL: trusted; else the addon's name) for all of it. The bytes aren't
  kept. W_OK when the file was read, warnings or not. W_ERR_CORRUPT or
  W_ERR_UNSUPPORTED when it isn't well-formed XML or its top element
  isn't <Ui>: it is skipped whole, and the same sentence is in the log.
  W_ERR_FULL: no memory.
- `WErr fxml_create(Fxml *fx, const char *type, const char *name, const char *parent, const char *inherits, const char *taint, const FxmlObject **out, WWhy *why);`
  What CreateFrame does with a template (705): makes an object of `type`
  (a frame, region or font tag), named `name` (NULL: unnamed; $parent
  worked out from the parent), parented to the frame named `parent`
  (NULL: none), inheriting `inherits` ("A, B" or NULL), all with
  `taint`. W_ERR_REFUSED: not a type made from XML. Template warnings go
  to the log as from XML, with the file "(CreateFrame)".
- `const FxmlNode *fxml_template(const Fxml *fx, const char *name);`
  The template of that name and its file, or NULL.
- `const FxmlObject *fxml_find(const Fxml *fx, const char *name);`
  The object last made with that name, or NULL.
- `int fxml_step_count(const Fxml *fx);`
  The steps loading took, from 0 to fxml_step_count - 1.
- `const FxmlStep *fxml_step_at(const Fxml *fx, int i);`
- `int fxml_warning_count(const Fxml *fx);`
  The warnings so far, from 0.
- `const FxmlWarning *fxml_warning_at(const Fxml *fx, int i);`
- `void fxml_warning_line(const FxmlWarning *w, char *out, size_t size);`
  One warning as a line of the interface log, "file:line sentence"
  ("file sentence" when about the whole file), into `out` of `size`
  bytes (cut short to fit, always ended).
- `const char *fxml_attr(const FxmlNode *n, const char *name);`
  An attribute of an element, by name ignoring case, or NULL.
- `const char *fxml_set(const FxmlObject *o, const char *name);`
  An object's attribute as it ends up, by name ignoring case, or NULL.
