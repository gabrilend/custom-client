/*
 * 083-w3i.h (issues 302, 1109) - a Warcraft III map's settings
 * (war3map.w3i): its water tint and fog
 *
 * For a general audience: a WC3 map keeps its settings in war3map.w3i:
 * its name, players, camera and so on, and in The Frozen Throne's maps
 * the colour its water is tinted and the fog its world is drawn in. This
 * reads only what the water and the fog need (issue 1109's water plan,
 * step 3, and step 1j): the map's flags, its main tileset, and its fog
 * and water tint. The rest of the file is passed over.
 *
 * The file (little-endian; a string ends at its zero byte), as the
 * published format notes and world-edit-to-execute's Lua reader give it:
 *   version (i32): 18 Reign of Chaos, 25 The Frozen Throne, 28 (1.31),
 *     31 and 33 (1.32 and later); saves (i32), editor version (i32);
 *     from version 28: the game's version, four i32
 *   name, author, description, players recommended (strings)
 *   camera bounds (8 floats), their complements (4 i32), playable width
 *     and height (i32), flags (u32), main tileset (one letter)
 *   from version 25 (Reign of Chaos's file ends its part here, with
 *   other fields this doesn't need): loading screen number (i32), its
 *   model, text, title, subtitle (strings), game data set (i32), the
 *   prologue's path, text, title, subtitle (strings); terrain fog style
 *   (i32), fog start and end distances and density (floats), fog colour
 *   (4 bytes: red, green, blue, alpha), weather (4 letters), sound
 *   environment (string), light environment (one letter), water tint (4
 *   bytes: red, green, blue, alpha)
 * The tint is used when flag W3I_FLAG_WATER_TINT is set (the editor's
 * "use water tinting colour"); the fog when flag W3I_FLAG_TERRAIN_FOG
 * is (the editor's "use terrain fog"), its style then the editor's list
 * counted from 0: linear, exponential, exponential squared. That reading
 * of the fog is issue 1109's decision 76, from the maps at hand: the
 * published notes and world-edit-to-execute's reader take style 0 for
 * no fog and name no flag for it (open question 13). Players, forces
 * and what follows aren't read.
 *
 * Window-free (READERS: the test program, the executable and
 * libwreaders.so). No handle: the answer is a plain record.
 */
#ifndef EG_W3I_H
#define EG_W3I_H
#include "013-errors.h"
#include <stddef.h>
#include <stdint.h>

#define W3I_FLAG_WATER_TINT 0x10000u
#define W3I_FLAG_TERRAIN_FOG 0x2000u

/* What is read. Field by field:
 *   version          the file's (18, 25, 28, 31, 33 ...)
 *   flags            the map's flags
 *   tileset_code     its main tileset's letter ('L' Lordaeron summer ...)
 *   has_environment  1 from version 25: the fields below are the map's;
 *                    0 for Reign of Chaos (no fog, white tint, untinted)
 *   fog_style        as the file gives it (0 linear, 1 exponential, 2
 *                    exponential squared: decision 76)
 *   fog_start, fog_end, fog_density   as the file gives them (WC3 units
 *                    from the eye; the density for the exponential styles)
 *   fog_on           1 when has_environment and the fog's flag is set
 *   fog_rgba         red, green, blue, alpha
 *   water_rgba       the tint, red, green, blue, alpha
 *   water_tinted     1 when has_environment and the tint's flag is set */
typedef struct {
    int      version;
    uint32_t flags;
    char     tileset_code;
    int      has_environment;
    int      fog_style;
    int      fog_on;
    float    fog_start, fog_end, fog_density;
    uint8_t  fog_rgba[4];
    uint8_t  water_rgba[4];
    int      water_tinted;
} W3iInfo;

/* Reads war3map.w3i's bytes into `out`. W_OK; W_ERR_UNSUPPORTED, with
 * why, for a version other than 18 or 25 and later; W_ERR_CORRUPT for a
 * file that ends before the fields above do, naming the field. */
WErr w3i_read(const unsigned char *s, size_t len, W3iInfo *out, WWhy *why);

#endif
