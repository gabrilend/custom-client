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

Not yet: choosing among the detail files by the bone limit (step 1;
this client always takes `00.skin`), ribbons (403), the model's own
lights (310), cameras inside models.

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
2. The WC3 model reader (MDX), filling the same model structure, the C
   twin of world-edit-to-execute's `src/parsers/mdx.lua` and
   `src/assets/gpu.lua` (docs/009, the second crosswalk). Its pictures
   already read: [107a](./completed/107a-wc3-pictures-blp1.md); its files
   come out of `war3.mpq` through 015.
3. Ribbon emitters (with 403) and model lights (with 310).
4. The drawing recipes above, in the doodad drawing code: the alpha-key
   cutoff from the batch's alpha, a true no-alpha add, a doubled
   modulate, fog colour per mode, the material flags. Tests (pictures
   with `scripts/run --shot`, filed by the run script): a glowing
   additive effect in thick fog fades to nothing rather than to grey; a
   modulate ×2 decal on grey ground leaves the ground unchanged where
   the decal is mid-grey.

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
