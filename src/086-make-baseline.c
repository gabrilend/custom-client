/*
 * 086-make-baseline.c (issues 105a, 1109) - the program that makes the
 * smallest baseline set
 *
 * For a general audience: run once (scripts/build builds it beside the
 * client), it writes the project's own stand-ins (a picture for every
 * ground, one post for every doodad, the water's pictures) into a folder
 * the client then asks after the Blizzard installs. It needs nothing but
 * the converter's two tables, from world-edit-to-execute.
 *
 * Usage: make-baseline FOLDER GROUND_TABLE MODEL_TABLE
 *   FOLDER        where the set goes: assets/baseline under the client's
 *                 folder by default (the assetBaseline setting); made when
 *                 it isn't there, made again over a set this made, and left
 *                 alone when it holds other files
 *   GROUND_TABLE  world-edit-to-execute's src/wowbuild/ground_textures.lua
 *   MODEL_TABLE   world-edit-to-execute's src/wowbuild/models.lua
 *
 * Exits 0 with the counts printed, 1 with the reason (085's) when refused.
 */
#include "085-baseline-set.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: make-baseline FOLDER GROUND_TABLE MODEL_TABLE\n"
                        "  (the tables: world-edit-to-execute's src/wowbuild/ground_textures.lua and models.lua)\n");
        return 1;
    }
    BaselineMade m;
    WWhy why = { 0 };
    WErr rc = baseline_set_make(argv[1], argv[2], argv[3], &m, &why);
    if (rc) { fprintf(stderr, "make-baseline: %s\n", why.text); return 1; }
    printf("the baseline set at %s: %d ground pictures, %d doodad models on the stand-in, %d water frames;\n"
           "%d files and %d manifest entries; left out: %d buildings, %d units' creatures, %d of other kinds, %d paths named twice\n",
           argv[1], m.grounds, m.models, m.water_frames, m.files, m.entries, m.buildings, m.units, m.other, m.same_path);
    return 0;
}
