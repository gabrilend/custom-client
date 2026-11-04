# 401 — The M2 Reader

*Came from: wow-chat-2026 173d (models and world objects) and 173g
(doodads), moved 2026-10-07.*

## Current behavior

Built (`src/022-m2.c`, window-free, in the reader library):
- An M2 model and its first view (`<name>00.skin`) read into points
  (position, normal, two sets of picture positions, four bones and
  weights each), pieces (the skin's submeshes), units (each piece's
  pictures, up to two layers, blend mode and flags, which set of picture
  positions each layer uses, picture-position animation, animated
  colour and transparency, the combine code), and the picture list (each
  a named file or a kind the game chooses at run time: skin, hair, cape,
  fur, creature skins).
- The model's collision triangles (header 216 and 224), simpler than
  what is drawn (`m2_collision`).
- Used by doodads and buildings' furnishings (`042-doodads.c`), figures
  (`041-figure.c`), and the reading-ahead thread.
- Tests: her model reads with her known height and triangle count; the
  doodads at Astranaar are placed exactly where the server's own
  collision tiles place them (710 of 710, within 0.0007 yards).

**Warcraft III's models** (step 2, built 2026-11-04, `src/061-mdx.c`,
window-free, in the reader library and `wreaders.h`): `mdx_read` reads
a whole `.mdx` (version 800, and the fields world-edit-to-execute's
Lua reads for later versions) into an `MdxModel`. Its `model` is the
same `M2Model` this reader fills, so a WC3 model can go where a WoW
model goes; beside it are what only WC3 has: sequences, global
sequences, materials and their layers, geosets as the file has them
(faces, matrix groups), geoset animations, every node (bones, helpers,
attachments, the nodes of lights, emitters and ribbons, events,
collision shapes), pivots, texture animations, and every animated value
(tracks and keys). The Lua's helpers have twins: `mdx_sequence`
(`mdx.sequence`), `mdx_node` (`mdx.nodes`), `mdx_sample`
(`mdx.sample`), `mdx_track`. The mapping into the shared structure is
under Decisions below. Shared structure extended (the M2 reader fills
the new fields too, and nothing else changes for it): a picture's wrap
flags (`M2Texture.flags`), a kind range for WC3's replaceable pictures
(`M2_TEXTURE_WC3`), a unit's alpha at rest (`M2Unit.alpha`, 1 for M2).

Checked:
- `tests/044-test-client.c`, "WC3's models (MDX)" (20 checks, before
  the archives, so they run without the game's files): a model written
  by the test (three sequences, two pictures, two materials, three
  geosets, two bones and a helper) reads to its known points, pieces,
  units, blends, flags, alphas at rest, nodes, pivots and a sampled
  move; cut short, unmarked, with an unknown track or a misnamed
  sub-chunk, it is refused as corrupt with a sentence.
- `tests/062-mdx-matches-lua.lua` (LuaJIT): both readers on the same
  bytes, compared field by field: everything `mdx.parse` gives, what
  `mdx.sequence`, `mdx.nodes` and `mdx.sample` answer (each track at
  each key, between and beyond them, with and without each sequence's
  bounds), and the pieces `gpu.lua`'s build makes (through stand-ins for
  its renderer and pictures) against the units, points and corners.
  Models made by the script with world-edit-to-execute's `binwrite.lua`:
  every chunk both read, versions 800 and 900, empty and bare models,
  and damaged copies (cut at every 97th byte, wrong marks). 2026-11-04:
  81 of 81 as expected (2844 sampled values identical; the damaged ones
  refused by both, or read the same where the cut fell between chunks).
  With the library built with the address and undefined-behaviour
  checkers: the same, and the C test block clean. The script's
  comparison was tested by planting bugs in a copy of the reader (a
  flag, the sphere's threshold, the last of two tracks, the last
  geoset animation, a sequence's bounds, cutting to whole triangles):
  each is caught.

Still needs WC3's own files (none exist here): the parity run over
every `.mdx` in `war3.mpq` and `War3x.mpq` (`luajit
tests/062-mdx-matches-lua.lua`, which runs it when world-edit-to-execute's
`wc3-installs/frozen-throne/` holds them, split over every core, as
050 does for pictures), on the owner's machine. Until then the match is
proven only on the made models. Then a WC3 model drawn: no drawing part
reads `mdx_read` yet (the doodads' and figures' loading is by `.m2`
name; team colour is 404's).

Not yet: choosing among the detail files by the bone limit (step 1;
this client always takes `00.skin`), ribbons (403), the model's own
lights (310), cameras inside models (WC3's CAMS too: passed over).

**Drawing what's read** (the doodad drawing code, `042-doodads.c`)
measured against the clean-room checklist's note 4.3
([docs/057](../docs/057-what-the-original-program-decides.md), filled
2026-10-16), the original's recipe for each blend mode:

| Mode | On screen | Drops pixels with alpha below | Fogged towards | Lit |
|---|---|---|---|---|
| 0 opaque | source | — | fog colour | yes |
| 1 alpha key | source | 0.878 × the batch's alpha | fog colour | yes |
| 2 alpha | source × a + destination × (1 − a) | 1/255 | fog colour | yes |
| 3 no-alpha add | source + destination | 1/255 | black | yes |
| 4 add | source × a + destination | 1/255 | black | yes |
| 5 modulate | source × destination | 1/255 | white | no |
| 6 modulate ×2 | twice source × destination | 1/255 | grey (128, 128, 128) | no |

Where this client differs: alpha key drops below 0.5 rather than 0.878
× the batch's alpha; mode 3 goes through raylib's additive blend, which
(to be confirmed) multiplies by alpha first, so it acts as mode 4;
mode 6 is drawn as a plain multiply, not doubled. Material flags (1
unlit, 2 no fog, 4 two-sided, 8 no depth test, 16 no depth write), the
other passes drawing modes 0 and 1 as mode 2, depth-only drawing, and
ribbons using the same mapping are also in note 4.3. Fogging added
effects towards black and multiplied ones towards white or grey is what
keeps them from glowing grey in fog.

## Intended behavior

Every M2 the game uses is read as the original reads it, version 264
(3.3.5a), into what the renderer, the animator and the collision need,
with nothing drawn by the reader itself. WC3 models join the same
renderer (docs/001, "One engine, two games"): a second reader beside this
one fills the same structures.

## Suggested implementation steps

1. The detail file chosen as the original chooses it (docs/057 note
   4.8, filled 2026-10-16): **not by distance**. A model loads one of
   `…00.skin` to `…03.skin` once and keeps it. The four allow 256, 64, 53
   and 21 bones; a model with fewer files has the last of that list; the
   first file whose allowance fits the card's bone limit is used (256,
   or for vertex-shader skinning the shader constants less 64 above 192
   or less 32, ÷ 3, at most 256); none fitting is a load error. This
   client has no bone limit of its own, so the limit is a setting in the
   options menu (answered question 1, [1001](./1001-the-options-menu.md)). Fading with
   distance is [307](./307-freefly-and-culling.md)'s.
1a. Textures combining (note 4.7): the batch's shader number built when
   the detail file loads (one operation, or one per texture up to two
   for models with header flag 8, with the sphere-mapped and
   second-coordinate-set marks; special numbers with the top bit set
   kept), the recipe named from the vertex and pixel parts that match
   the original's shipped shader files, modulate-modulate where there's
   no recipe, each texture's wrap from its own flags, sphere mapping in
   place of texture animation. This client takes the shader number's
   lowest three bits (`m2_combine`). One **quirk**, with a correction
   switch off by default: first operation 4 with second 1 uses the same
   recipe as first 1 with second 4, swapping the two textures' roles;
   the correction uses a "Mod2x then Mod" recipe where one exists.
2. **Built 2026-11-04** (above; the run over `war3.mpq` waits for the
   owner's machine). The WC3 model reader (MDX), filling the same model
   structure, the C twin of world-edit-to-execute's
   `src/parsers/mdx.lua` and `src/assets/gpu.lua` (docs/009, the second
   crosswalk). Its pictures already read:
   [107a](./completed/107a-wc3-pictures-blp1.md); its files come out of
   `war3.mpq` through 015.
3. Ribbon emitters (with 403) and model lights (with 310).
4. The drawing recipes above, in the doodad drawing code: the alpha-key
   cutoff from the batch's alpha, a true no-alpha add, a doubled
   modulate, fog colour per mode, the material flags. Tests (pictures
   with `scripts/run --shot`, filed by the run script): a glowing
   additive effect in thick fog fades to nothing rather than to grey; a
   modulate ×2 decal on grey ground leaves the ground unchanged where
   the decal is mid-grey.

## Decisions

Step 2's mapping of a WC3 model into the shared model structure,
decided 2026-11-04 in background work (the owner asked for questions to
be answered and recorded rather than waited on). Each is the smallest
mapping that keeps the M2 reader and its users unchanged; any can be
revisited.

1. **One structure for what is drawn, WC3's own beside it.** `MdxModel`
   holds an `M2Model` and the rest next to it, rather than widening
   `M2Model` with WC3's skeleton and tracks. The renderer only needs
   points, pieces, units and pictures; WC3's skeleton (matrix groups,
   pivots, nodes by object id) and tracks are shaped unlike M2's bones,
   and how one posing serves both is 402 step 1's to decide. Putting
   them in `M2Model` now would guess at that.
2. **Points: every geoset's in one list.** M2 keeps one list of points
   and pieces that index it; WC3 gives each geoset its own. So the
   geosets' points are laid end to end (each geoset says where its
   own are) and triangles hold model point numbers, as M2's do.
   Normals the file doesn't give face up, as gpu.lua does. Two sets of
   picture positions fit M2's two (`u, v`, `u2, v2`); a third or later
   is skipped (WC3's own models have one).
3. **A piece a geoset, its id the geoset's number.** M2's piece ids
   (group × 100 + variant) choose a character's hair and clothes; WC3
   has no such choice, and the number ties a piece back to its geoset
   (and to the geoset animations that name it).
4. **Triangles: kind 4 only, cut to a whole number.** gpu.lua's rule,
   and WC3 uses no other kind. A geoset with no points, more than 65535,
   or fewer than three corners draws nothing (gpu.lua's rule too: WC3's
   corners are 16-bit). A corner past its geoset's points becomes the
   geoset's first point, as `m2_read` does; gpu.lua hands it to the card
   as it is, which reads past the mesh. This is the one place the C is
   meant to differ, and the parity check expects it.
5. **A unit a geoset and layer.** WC3 materials stack any number of
   layers, each with its own blend; M2's unit holds two layers joined by
   one combine code. So each layer is its own unit, drawn in order, as
   gpu.lua makes each its own part (`layers` 1, `shader` 0). A geoset
   whose material is missing gets gpu.lua's plain layer. `unit_layer`
   says which layer each unit came from, for its animated values.
6. **Filter modes are M2's blends, one for one.** WC3's 0 none,
   1 transparent, 2 blend, 3 additive, 4 add alpha, 5 modulate,
   6 modulate 2x are M2's 0 opaque, 1 alpha key, 2 alpha, 3 no-alpha
   add, 4 add, 5 modulate, 6 modulate × 2, so the doodads' drawing
   recipes (the table above) apply as they are. WC3's alpha-key cutoff
   (0.75 in the open WC3 model viewers; not yet checked against the
   original) is not M2's 0.878 × alpha; which one a WC3 model draws
   with is the drawing's to settle with step 4.
7. **Shading flags as M2's material flags**: unshaded → unlit (0x1),
   unfogged → no fog (0x2), two sided → 0x4, no depth test → 0x8, no
   depth set → 0x10 (the same meanings, other bits). Sphere environment
   mapping (shading 2) is M2's reflection, picture-position set -1.
8. **Replaceable pictures get their own kinds** (`M2_TEXTURE_WC3` + the
   id). WC3's ids collide with M2's run-time kinds (WC3's 1, team
   colour, is M2's 1, a character's skin; `041-figure.c` paints kind 1
   with the skin). Offset, they never read as the game's, and every
   current user skips a kind it doesn't know. Which picture stands in
   (gpu.lua's table of trees and cliffs, a white picture tinted for team
   colour, a soft glow for team glow, grey for a missing one) is the
   drawing's, so the reader keeps only the id and the path as given.
9. **Wrap flags kept for both games** (`M2Texture.flags`): WC3's 1 and
   2 (wrap across, down) are M2's same two bits, the M2 reader skipped
   them, and gpu.lua uses them when it makes the picture.
10. **Alpha at rest on the unit** (`M2Unit.alpha`): the layer's alpha at
    the start of Stand times its geoset animation's, gpu.lua's value for
    a model drawn unposed. Without it a WC3 model drawn before 402 shows
    its death and alternate pieces over its body. M2 sets 1, since its
    tints come through its own tracks. WC3's tracks stay WC3's
    (`colour` and `fade` -1): their keys are milliseconds on one
    timeline cut into sequences, not M2's per-animation tracks, and
    joining the two is 402's.
11. **Sequences stay in milliseconds, on the WC3 side.** M2's sequences
    are read by 023-anim from the file, not kept in `M2Model`, so there
    is no shared place to convert them into. They are kept as the file
    has them (start and end on the model's timeline) for 402.
12. **Stricter than the Lua on damaged files.** The C refuses a record
    that runs past its chunk, a chunk that holds less than its records
    read, pivots that aren't whole points, and a record whose size
    can't move the reading on (the Lua loops forever on that last one,
    and reads past the end of its buffer on a short 16- or 8-bit list).
    Every read checks the end and every count is checked against the
    bytes left before memory is taken. On whole files the two agree.
13. **Cameras (CAMS) passed over.** The Lua reads them; nothing draws
    them, and cameras inside models are already listed as not yet.
    Their tag is kept in the chunk list, so the two agree on it.

## Open questions

None.

## Answered

1. ~~**Which detail file.**~~ A setting the player chooses (2026-10-20).
   The original picks a model's detail file by the graphics card's bone
   limit (docs/057 note 4.8). On a card that skins with vertex shaders
   and has 256 shader constants, a common case, the limit is 64 bones,
   so a model with all four files loads `…01.skin`, the second,
   slightly simpler one. This client bends models from a bone picture on
   the graphics card, which has no such limit, and so far always loads
   `…00.skin`, the fullest. Asked whether to copy what most players saw
   or keep what the modellers built, the owner: "Whoa. That should be a
   UI option." So:
   - **The setting:** the bone limit the detail choice pretends to, one
     of the original's four allowances (256, 64, 53, 21). The first file
     whose allowance fits is loaded, by the same rule as step 1. It
     lives in the Effects panel ([1001](./1001-the-options-menu.md)).
     It's the project's own setting, since the original has none.
   - **Default: 64**, so models look as they did on a typical player's
     machine (original first). 256 gives the fullest file.
   - **When it takes effect:** a model keeps its file once loaded, as in
     the original. A change waits in the options menu's list until
     Apply, which unloads and reloads every model. Beforehand, a
     preview beside the setting shows the waiting limit on the dummies
     in the options menu's dressing room, a camera view into a void in
     the world (the owner, 2026-10-20;
     [1001](./1001-the-options-menu.md)). So the
     reader needs to unload all models and load them again with a new
     limit, and to load one model on its own, for the preview, at a
     limit the world isn't using.
   - **Tests:** a model with four files loads `…01.skin` at 64 and
     `…00.skin` at 256; a model with two files loads `…01.skin` at 21.

## Related

- [402](./402-skeletons-and-animation.md), [403](./403-particles.md),
  [404](./404-characters-and-equipment.md), [305](./305-wmo-renderer.md).
