/*
 * 067-test-map.h (issue 1109) - a map handed in by the editor
 *
 * For a general audience: world-edit-to-execute's map editor has a Test
 * Map button (F5). It converts the map being edited into a build
 * folder, starts a local AzerothCore with it, and opens this client
 * straight into that map, with no menus:
 *
 *   w-client --test-map <folder> --receipt <file> [--camera <x>,<y>]
 *            [--realm <host>:<port>] [--account <name>] [--headless <moves>]
 *
 * (and the client's own --maps and --client, which scripts/run adds).
 * This reads those options and checks them before any window opens:
 *
 *   --test-map  the converted map's build folder; for this run only it
 *               is put in front of the overlays the settings name
 *               (assetOverlays), so its files win over theirs and over
 *               the original game's archives. The settings file is not
 *               changed.
 *   --receipt   where the goodbye receipt goes when the client ends
 *               (068-goodbye.h); the editor reads it back
 *   --camera    where the camera starts, world x and y in yards, a comma
 *               between (12.5,-300); without it, the map's own start
 *   --realm     the local AzerothCore's login server (default
 *               127.0.0.1:3724, its usual port)
 *   --account   the account to log in with
 *   --headless  no window: the start, the figures' lookups and the
 *               scripted moves in the file named (a Lua data file,
 *               089-headless-run.h), then the receipt with their counts,
 *               and the client ends (1109 step 1n)
 *
 * The password is never on the command line, where every user of the
 * machine can read it: it is read from the environment variable
 * W_CLIENT_PASSWORD, and a test-map run without it (or with it empty)
 * stops before the window opens, with a receipt saying why.
 *
 * Every option the client takes is checked here, its own and the test
 * map's: one the client doesn't know, one given without its values, or
 * a test-map option outside a test-map run, is refused by name, so a
 * misspelt option stops the start instead of being passed over.
 *
 * Window-free: in the test program and the executable.
 */
#ifndef EG_TEST_MAP_H
#define EG_TEST_MAP_H

#include "013-errors.h"
#include "014-settings.h"
#include "015-mpq.h"
#include "060-resolver.h"

/* One of the client's own options and how many values follow it; a list
 * of them ends with a NULL name. A value is missing when the arguments
 * run out, or when the next argument starts with "--". */
typedef struct {
    const char *name;
    int         values;
} ClientOption;

#define TEST_MAP_REALM_DEFAULT_HOST "127.0.0.1"
#define TEST_MAP_REALM_DEFAULT_PORT 3724
#define TEST_MAP_PASSWORD_VARIABLE  "W_CLIENT_PASSWORD"

/* The options of a test-map run (the handle: docs/001, "Handles"). */
typedef struct TestMap TestMap;

/* Reads argv[1] to argv[argc - 1]. `others` lists the client's own
 * options (they are checked, not read: 043-main.c reads them); `password`
 * is W_CLIENT_PASSWORD's value, NULL when it isn't set.
 *   W_OK, *out NULL: not a test-map run (no --test-map), every option known
 *   W_OK, *out set:  a test-map run, every option good
 *   on failure:      the code and why; W_ERR_REFUSED for an unknown option,
 *                    a missing value, a bad camera, realm or account, an
 *                    option given twice, or a test-map option outside a
 *                    test-map run; W_ERR_NOT_FOUND for no --receipt or no
 *                    password. *out is still set when the run is a test-map
 *                    run whose --receipt was read, so the caller can write
 *                    the receipt saying why (its folder may be "" when
 *                    --test-map lacked it); NULL otherwise.
 * Free with test_map_free, which also wipes the password from memory. */
WErr test_map_parse(int argc, char *const *argv, const ClientOption *others, const char *password, TestMap **out, WWhy *why);
void test_map_free(TestMap *tm);

/* What was asked for: the folder and the receipt as given; the camera
 * (1 with x and y when --camera was given, else 0); the realm; the
 * account ("" when none was given); the password. */
const char *test_map_folder(const TestMap *tm);
const char *test_map_receipt(const TestMap *tm);
int         test_map_camera(const TestMap *tm, double *x, double *y);
const char *test_map_realm_host(const TestMap *tm);
int         test_map_realm_port(const TestMap *tm);
const char *test_map_account(const TestMap *tm);
const char *test_map_password(const TestMap *tm);
/* --headless's moves file as given, "" when the run has a window. */
const char *test_map_headless(const TestMap *tm);

/* Where the resolver looks for this run: the settings' place
 * (resolver_place_from_settings, with `assets_dir` and `chain`), and the
 * map's folder put first among its overlays, the settings' overlays
 * after it in their order. `assets_dir` may be NULL when the settings
 * name no packs or overlays. W_OK with `*out`; W_ERR_REFUSED when the
 * settings name folders and `assets_dir` is NULL, or the map has no folder;
 * W_ERR_FULL when the overlays are full already; the
 * settings' own failures. The settings store is not changed. */
WErr test_map_place(const TestMap *tm, const Settings *st, const char *assets_dir, MpqChain *chain, ResolverPlace *out, WWhy *why);

#endif
