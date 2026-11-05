# 086-make-baseline

*Written by `scripts/generate-info` from `086-make-baseline.c`'s opening comment (a program: it offers no functions, only how it is run); don't edit, re-run it.*

086-make-baseline.c (issues 105a, 1109) - the program that makes the
smallest baseline set

For a general audience: run once (scripts/build builds it beside the
client), it writes the project's own stand-ins (a picture for every
ground, one post for every doodad, the water's pictures) into a folder
the client then asks after the Blizzard installs. It needs nothing but
the converter's two tables, from world-edit-to-execute.

Usage: make-baseline FOLDER GROUND_TABLE MODEL_TABLE
  FOLDER        where the set goes: assets/baseline under the client's
                folder by default (the assetBaseline setting); made when
                it isn't there, made again over a set this made, and left
                alone when it holds other files
  GROUND_TABLE  world-edit-to-execute's src/wowbuild/ground_textures.lua
  MODEL_TABLE   world-edit-to-execute's src/wowbuild/models.lua

Exits 0 with the counts printed, 1 with the reason (085's) when refused.
