/*
 * 101-loading-screen.h (issue 1109 step 1y) - a map's loading screen
 *
 * For a general audience: while a map loads, the game shows a picture
 * for it. Which picture is said by the tables: the map's row (Map.dbc)
 * names a row of LoadingScreens.dbc, and that row names the picture's
 * file. World-edit-to-execute's converter writes all three for a
 * converted map (its src/wowbuild/loadscreen.lua: the map's own preview
 * picture, or its ground drawn from above, as a 512 x 512 BLP2 with
 * every smaller copy). This finds the picture by those rows and decodes
 * it, with no window, so the window (043) can show it while the world
 * loads and a run with no window (089) can say which it was.
 *
 * The columns are found by name in the layouts (097: Map's
 * LoadingScreenID, LoadingScreens' Name, FileName, HasWideScreen), held
 * equal to the converter's by tests/100-layouts-match-converter.lua. A
 * Map copy too narrow for LoadingScreenID gives no loading screen rather
 * than refusing the table (097 leaves that column unmarked). The
 * picture comes through the resolver (060) when a test map's is open,
 * else the archives, never a placeholder: no picture is no loading
 * screen, not a checker. HasWideScreen is read and kept; what the
 * original does with it (a wide copy of the picture) is recalled, so
 * nothing uses it yet. Whether the original shows a BLP2 of encoding 3
 * (the converter's) is unchecked: this client's reader does.
 *
 * Window-free, no state (CORE: the test program and the executable).
 */
#ifndef EG_LOADING_SCREEN_H
#define EG_LOADING_SCREEN_H

#include "013-errors.h"
#include "015-mpq.h"
#include "017-blp.h"
#include "060-resolver.h"
#include "074-tables.h"

#include <stdint.h>

/* A map's loading screen, owned until loading_screen_free:
 *   id           its LoadingScreens row (the map's LoadingScreenID)
 *   name, file   that row's Name and FileName (the picture's path)
 *   wide         its HasWideScreen, read, not used (see above)
 *   row_from     where the LoadingScreens row came from (074's place)
 *   picture_from where the picture came from (060's place)
 *   picture      the picture, full size, RGBA, rows top to bottom
 *   levels       how many copies the file holds, full size first */
typedef struct {
    uint32_t    id;
    char        name[128];
    char        file[260];
    uint32_t    wide;
    AssetSource row_from, picture_from;
    BlpImage    picture;
    int         levels;
} LoadingScreen;

/* Map `map_id`'s loading screen, through `tables` and, for the picture,
 * `r` (or the archives alone, `chain`, with no resolver). W_OK with
 * `*out`; otherwise `*out` holds no picture and:
 *   W_ERR_NOT_FOUND  no Map row; a Map copy too narrow for LoadingScreenID;
 *                    the row names none (LoadingScreenID 0);
 *                    no LoadingScreens row of that number; its FileName
 *                    empty; or no place has the picture (each named)
 *   W_ERR_CORRUPT, W_ERR_UNSUPPORTED  the picture won't decode (017's)
 *   074's refusals   a table too narrow for these columns */
WErr loading_screen_find(Tables *tables, Resolver *r, MpqChain *chain, int map_id, LoadingScreen *out, WWhy *why);
void loading_screen_free(LoadingScreen *s);

#endif
