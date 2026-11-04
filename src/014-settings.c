/*
 * 014-settings.c (issue 173b1) - the settings store; see 014-settings.h
 *
 * For a general audience: the table below is the client's whole list of
 * settings. A feature that needs a setting adds its row here in the same
 * change that builds the feature (173b); the Options menu (173b2) will
 * draw itself from these rows.
 */
#include "014-settings.h"
#include "013-errors.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the declarations
 * The rows, in menu order within each section. Ranges and steps are the
 * original's (docs/custom-client-stock-options.md); defaults are this
 * client's own (owner, 2026-10-06: "Don't worry too much about what the
 * defaults on the original client are").
 *
 * cameraDistanceMaxFactor: the zoom-out limit is ZOOM_BASE (20 yards, in
 *   043-main.c) times this, so 2 keeps the 40 yards first light had (173b4).
 * cameraYawMoveSpeed: how fast a mouse drag turns the camera. 180 keeps
 *   first light's 0.006 radians a pixel; the rate scales with it (90 is
 *   half as fast, 270 one and a half times).
 * cameraSmoothStyle: the following style (173m). Its values must match
 *   the rows of 031-camera.c's styles table; the camera stops the client if
 *   they ever disagree. "1" is the owner's choice (2026-10-06).
 * cameraYawSmoothSpeed: degrees a second the camera drifts back behind her.
 * cameraTerrainTilt, cameraWaterCollision, cameraPivot, cameraBobbing:
 *   the Camera panel's Follow Terrain (off), Water Collision (on), Smart
 *   Pivot (on) and Head Bob (off): what each does is in 031-camera.c and
 *   043-main.c (173m); defaults are this client's.
 * gxVSync, gxWindow, gxMaximize, windowResizeLock: the Resolution
 *   panel's tick boxes (173b5), the original's names and words. Windowed
 *   and Maximized are read when the window opens (Raylib picks
 *   fullscreen and borders then); Vertical Sync and Disable Resize are
 *   applied live by 043-main.c. Windowed Mode's original tooltip also speaks
 *   of the desktop's gamma, which this client doesn't change, so that
 *   sentence is left out. Defaults: windowed, vsync on, as the client
 *   opened before these existed.
 * particleDensity: Particle Density (173b6): every emitter's birth rate
 *   times this (024-particles.c); 1, all of them, is this client's default.
 * textureFilteringMode: Texture Filtering (173b6): 0 bilinear, 1
 *   trilinear (the default: every picture was trilinear before it), 2 to
 *   5 anisotropic 2x to 16x (036-texture.c). Read at start, as the original.
 * farclip: the view distance, yards (173b6): the far plane, how far the
 *   fog may reach, and how many tiles load round her (one tile covers
 *   533 yards). 507 keeps the 3 x 3 the client had before; the range and
 *   step are the original's.
 * assetPacks, assetOverlays, assetsStrict, assetsDistinctness: the asset
 *   resolver's (issue 105a, 060-resolver.h), with no twin in the
 *   original. The two lists name folders under the profile's
 *   assets/packs and assets/overlays, commas between, highest priority
 *   first; names, not paths, so a profile's settings never point outside
 *   its own assets and the 63 characters a value holds go far. None by
 *   default: every asset comes from the player's archives, as before.
 *   Strict mode is off by default (a missing asset is a warning and a
 *   placeholder). The distinctness threshold, 0.5 for now, is a stand-in
 *   until the owner picks one (world-edit-to-execute's open question 10):
 *   a pack asset whose latest similarity score is at or above it counts
 *   as replaced.
 */
static const char *check_resolution(const char *value);
static const char *check_folder_names(const char *value);

static const SettingChoice CAMERA_STYLES[] = {
    { "Only horizontal when moving", "1" },
    { "Only when moving",            "4" },
    { "Always adjust camera",        "2" },
    { "Never adjust camera",         "0" },
    { NULL, NULL },
};

static const SettingDecl DECLS[] = {
    { .name = "gxResolution",            .section = "Resolution", .kind = SETTING_TEXT,   .def = "1280x720",
      .check = check_resolution, .restart = 1,
      .label = "Resolution", .explain = "The window's width and height in pixels, typed as WIDTHxHEIGHT (up to 7680x4320, ultrawide included)." },
    { .name = "gxVSync",                 .section = "Resolution", .kind = SETTING_TOGGLE, .def = "1",
      .label = "Vertical Sync", .explain = "Synchronizes your framerate to some fraction of your monitor's refresh rate. Enable this if you see excessive screen tearing in game." },
    { .name = "gxWindow",                .section = "Resolution", .kind = SETTING_TOGGLE, .def = "1", .restart = 1,
      .label = "Windowed Mode", .explain = "Check to play in a non-fullscreen window." },
    { .name = "gxMaximize",              .section = "Resolution", .kind = SETTING_TOGGLE, .def = "0", .restart = 1,
      .label = "Maximized", .explain = "Check to maximize window and remove borders." },
    { .name = "windowResizeLock",        .section = "Resolution", .kind = SETTING_TOGGLE, .def = "0",
      .label = "Disable Resize", .explain = "Enable this to prevent resizing the game window." },
    { .name = "farclip",                 .section = "Effects",    .kind = SETTING_SLIDER, .def = "507",
      .min = 177, .max = 1277, .step = 110,
      .label = "View Distance", .explain = "Changes how far you can see. Decreasing this may greatly improve performance." },
    { .name = "particleDensity",         .section = "Effects",    .kind = SETTING_SLIDER, .def = "1",
      .min = 0.1, .max = 1, .step = 0.1,
      .label = "Particle Density", .explain = "Controls the number of particles used in effects caused by spells, fires, etc. Decrease to improve performance." },
    { .name = "textureFilteringMode",    .section = "Effects",    .kind = SETTING_SLIDER, .def = "1", .restart = 1,
      .min = 0, .max = 5, .step = 1,
      .label = "Texture Filtering", .explain = "Increases texture sharpness, particularly for textures viewed at an angle.  Decrease to improve performance." },
    { .name = "cameraDistanceMaxFactor", .section = "Camera",     .kind = SETTING_SLIDER, .def = "2",
      .min = 1,  .max = 2,   .step = 0.1,
      .label = "Max Camera Distance", .explain = "Adjust the maximum distance the camera will follow behind you." },
    { .name = "cameraYawSmoothSpeed",    .section = "Camera",     .kind = SETTING_SLIDER, .def = "180",
      .min = 90, .max = 270, .step = 10,
      .label = "Auto Follow Speed", .explain = "Adjust the camera movement speed in Always and Smart following styles." },
    { .name = "cameraSmoothStyle",       .section = "Camera",     .kind = SETTING_CHOICE, .def = "1",
      .choices = CAMERA_STYLES,
      .label = "Camera Following Style", .explain = "How the camera returns behind your character on its own." },
    { .name = "cameraTerrainTilt",       .section = "Camera",     .kind = SETTING_TOGGLE, .def = "0",
      .label = "Follow Terrain", .explain = "Automatically change the camera angle based on the terrain. If your character climbs a slope, the camera will rotate upwards; if you descend, it will rotate downwards." },
    { .name = "cameraWaterCollision",    .section = "Camera",     .kind = SETTING_TOGGLE, .def = "1",
      .label = "Water Collision", .explain = "Set the camera to be above water when your character is above water, and below water when your character is below water." },
    { .name = "cameraPivot",             .section = "Camera",     .kind = SETTING_TOGGLE, .def = "1",
      .label = "Smart Pivot", .explain = "Lets you free look when the camera is on the ground." },
    { .name = "cameraBobbing",           .section = "Camera",     .kind = SETTING_TOGGLE, .def = "0",
      .label = "Head Bob", .explain = "Simulate head bobbing when in first-person perspective." },
    { .name = "mouseInvertPitch",        .section = "Mouse",      .kind = SETTING_TOGGLE, .def = "0",
      .label = "Invert Mouse", .explain = "Invert the way that the mouse affects camera pitch." },
    { .name = "cameraYawMoveSpeed",      .section = "Mouse",      .kind = SETTING_SLIDER, .def = "180",
      .min = 90, .max = 270, .step = 10,
      .label = "Mouse Look Speed", .explain = "Adjust the speed at which you turn the camera when using mouse look." },
    { .name = "assetPacks",              .section = "Assets",     .kind = SETTING_TEXT,   .def = "", .check = check_folder_names, .restart = 1,
      .label = "Replacement Packs", .explain = "Folders under assets/packs whose models, pictures and sounds stand in for the original game's, highest priority first, commas between." },
    { .name = "assetOverlays",           .section = "Assets",     .kind = SETTING_TEXT,   .def = "", .check = check_folder_names, .restart = 1,
      .label = "Overlays", .explain = "Folders under assets/overlays (a converted Warcraft III map, for one) read before the original game's archives, highest priority first, commas between." },
    { .name = "assetsStrict",            .section = "Assets",     .kind = SETTING_TOGGLE, .def = "0", .restart = 1,
      .label = "Strict Assets", .explain = "Treat any asset missing from every pack, folder and archive as an error, instead of drawing a placeholder." },
    { .name = "assetsDistinctness",      .section = "Assets",     .kind = SETTING_SLIDER, .def = "0.5", .min = 0, .max = 1, .step = 0.01, .restart = 1,
      .label = "Distinctness Threshold", .explain = "The similarity score from which a pack's asset counts as replaced: 0 is the original itself, 1 unrelated to it." },
};
#define DECL_COUNT ((int)(sizeof DECLS / sizeof DECLS[0]))

/* a store: every setting's value, in the declarations' order */
struct Settings { Setting values[DECL_COUNT]; };
/* }}} */

/* {{{ check_resolution: WIDTHxHEIGHT, from 640x480 to 7680x4320 */
int settings_resolution(const char *value, int *width, int *height)
{
    char tail;
    if (sscanf(value, "%dx%d%c", width, height, &tail) != 2) return 1;
    return (*width < 640 || *width > 7680 || *height < 480 || *height > 4320) ? 1 : 0;
}
static const char *check_resolution(const char *value)
{
    int w, h;
    return settings_resolution(value, &w, &h) ? "not a resolution from 640x480 to 7680x4320, written WIDTHxHEIGHT" : NULL;
}
/* }}} */

/* {{{ check_folder_names: "", or folder names with commas between (no
 * slashes, no "." or "..", nothing empty between two commas) */
static const char *check_folder_names(const char *value)
{
    if (!*value) return NULL;
    const char *p = value;
    for (;;) {
        const char *start = p;
        while (*p && *p != ',') {
            unsigned char c = (unsigned char)*p++;
            if (!(isalnum(c) || c == '-' || c == '_' || c == '.')) return "not folder names (letters, digits, - _ and .) with commas between";
        }
        size_t len = (size_t)(p - start);
        if (len == 0) return "a folder name left empty between commas";
        if ((len == 1 && start[0] == '.') || (len == 2 && start[0] == '.' && start[1] == '.')) return "a folder name of . or .., which would leave the folder";
        if (!*p) return NULL;
        ++p;
    }
}
/* }}} */

/* {{{ settings_find, settings_count, settings_at
 * A walk down the table by name. The table is small (tens of rows, a few
 * hundred when every panel is built), and a few lookups a frame cost
 * nothing next to drawing; a hash can replace the walk if that changes. */
const Setting *settings_find(const Settings *st, const char *name)
{
    for (int i = 0; i < DECL_COUNT; ++i)
        if (!strcmp(DECLS[i].name, name)) return &st->values[i];
    return NULL;
}
int settings_count(void) { return DECL_COUNT; }
const Setting *settings_at(const Settings *st, int i) { return (i >= 0 && i < DECL_COUNT) ? &st->values[i] : NULL; }
/* }}} */

/* {{{ static accept: a value held to its declaration
 * Fills `out` (number and text) when `value` is allowed for `d`; otherwise
 * returns why not. Numbers are rewritten in one form (%g), so "180.0"
 * and "180" are the same value and compare equal to the default. */
static const char *accept(const SettingDecl *d, const char *value, Setting *out)
{
    char *end;
    double n = 0;
    char text[64];
    if (strlen(value) >= sizeof text) return "too long (63 characters at most)";
    switch (d->kind) {
    case SETTING_TOGGLE:
        if (strcmp(value, "0") && strcmp(value, "1")) return "not 0 or 1";
        n = value[0] - '0';
        snprintf(text, sizeof text, "%s", value);
        break;
    case SETTING_SLIDER:
        n = strtod(value, &end);
        if (end == value || *end) return "not a number";
        if (n < d->min || n > d->max) return "out of its range";
        snprintf(text, sizeof text, "%g", n);
        break;
    case SETTING_CHOICE: {
        const SettingChoice *c = d->choices;
        while (c && c->value && strcmp(c->value, value)) ++c;
        if (!c || !c->value) return "not one of its choices";
        n = strtod(value, &end);               /* a numeric choice's number; 0 for a word */
        if (end == value || *end) n = 0;
        snprintf(text, sizeof text, "%s", value);
        break; }
    case SETTING_TEXT: {
        const char *why = d->check ? d->check(value) : NULL;
        if (why) return why;
        snprintf(text, sizeof text, "%s", value);
        break; }
    }
    out->decl = d;
    out->num = n;
    memcpy(out->text, text, sizeof text);
    return NULL;
}
/* }}} */

/* {{{ settings_new, settings_free
 * A default that its own declaration refuses is a mistake in the table:
 * the client stops and names it. */
void settings_free(Settings *st) { free(st); }
Settings *settings_new(void)
{
    Settings *st = calloc(1, sizeof *st);
    for (int i = 0; i < DECL_COUNT; ++i) {
        const char *why = accept(&DECLS[i], DECLS[i].def, &st->values[i]);
        if (why) {
            fprintf(stderr, "014-settings.c: the default \"%s\" of %s is refused by its own declaration: %s\n",
                    DECLS[i].def, DECLS[i].name, why);
            exit(1);
        }
    }
    return st;
}
/* }}} */

/* {{{ settings_set */
WErr settings_set(Settings *st, const char *name, const char *value, WWhy *why)
{
    for (int i = 0; i < DECL_COUNT; ++i) {
        if (strcmp(DECLS[i].name, name)) continue;
        Setting next;
        const char *no = accept(&DECLS[i], value, &next);
        if (no) {
            if (DECLS[i].kind == SETTING_SLIDER)
                w_fail(why, W_ERR_REFUSED, "%s \"%s\": %s (%g to %g)", name, value, no, DECLS[i].min, DECLS[i].max);
            else
                w_fail(why, W_ERR_REFUSED, "%s \"%s\": %s", name, value, no);
            return why->code;
        }
        st->values[i] = next;
        return 0;
    }
    w_fail(why, W_ERR_REFUSED, "no setting named %s", name);
    return why->code;
}
/* }}} */

/* {{{ settings_load
 * Each line is SET name "value", as the original's Config.wtf writes it.
 * The quotes are required; a value can't hold a quote. */
int settings_load(Settings *st, const char *path, FILE *report)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(report, "settings: no file at %s yet; every setting is at its default\n", path);
        return 0;
    }
    char line[512], name[128], value[256];
    WWhy why;
    int number = 0, skipped = 0;
    while (fgets(line, sizeof line, f)) {
        ++number;
        char *s = line;
        while (*s == ' ' || *s == '\t') ++s;
        if (*s == '\n' || *s == '\r' || *s == '\0' || *s == '#') continue;
        char tail[2];
        if (sscanf(s, "SET %127s \"%255[^\"]\" %1s", name, value, tail) != 2) {
            fprintf(report, "settings: %s line %d: not SET name \"value\"; skipped\n", path, number);
            ++skipped;
            continue;
        }
        if (settings_set(st, name, value, &why)) {
            fprintf(report, "settings: %s line %d: %s; skipped\n", path, number, why.text);
            ++skipped;
        }
    }
    fclose(f);
    return skipped;
}
/* }}} */

/* {{{ settings_save */
WErr settings_save(const Settings *st, const char *path, WWhy *why)
{
    FILE *f = fopen(path, "w");
    if (!f) { w_fail(why, W_ERR_IO, "can't write %s (does its folder exist?)", path); return why->code; }
    for (int i = 0; i < DECL_COUNT; ++i) {
        Setting def;
        accept(&DECLS[i], DECLS[i].def, &def);      /* accepted at init, so never refused here */
        if (strcmp(st->values[i].text, def.text)) fprintf(f, "SET %s \"%s\"\n", DECLS[i].name, st->values[i].text);
    }
    if (fclose(f)) { w_fail(why, W_ERR_IO, "writing %s failed", path); return why->code; }
    return 0;
}
/* }}} */

/* {{{ settings_num_at, settings_text_at */
static const Setting *must(const Settings *st, const char *name, const char *file, int line)
{
    const Setting *s = settings_find(st, name);
    if (!s) {
        fprintf(stderr, "%s:%d asks for a setting named %s, which 014-settings.c doesn't declare\n", file, line, name);
        exit(1);
    }
    return s;
}
double settings_num_at(const Settings *st, const char *name, const char *file, int line) { return must(st, name, file, line)->num; }
const char *settings_text_at(const Settings *st, const char *name, const char *file, int line) { return must(st, name, file, line)->text; }
/* }}} */
