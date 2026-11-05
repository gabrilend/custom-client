/*
 * 058-dressing-room.c (issue 410) - pictures of items: each item's icon,
 * and a character wearing it, for pages outside the game
 *
 * For a general audience: a page about the game's items (wow-chat-2026's
 * list of the tabards a death knight's oath accepts was the first) needs
 * pictures of them, and only the game's own files hold those. This
 * program is given a list of items and writes, for each, the item's icon
 * as the game shows it in bags, and a picture of a character wearing it,
 * from the front and from behind. No world is loaded: the character
 * stands alone on a see-through backdrop, under one fixed light, drawn
 * into a picture held on the graphics card (not read off the screen), so
 * every picture comes out the same size whatever the window manager does
 * with the window, which stays hidden.
 *
 * Usage: dressing-room --client DIR --list FILE --out DIR [--receipt FILE]
 *                      [--race ID] [--sex 0|1] [--class ID] [--hair N] [--hair-color N]
 *   --client   the original 3.3.5a client's Data folder
 *   --list     one picture per line: NAME SLOT:LOOK (NAME the files'
 *              name, letters, digits, - and _; SLOT a slot as the
 *              client's --wear names it; LOOK an item look, an
 *              ItemDisplayInfo id). Blank lines and lines starting with
 *              # are skipped.
 *   --out      the folder the pictures go to: NAME-icon.png (64 x 64),
 *              NAME-front.png and NAME-back.png (WIDTH x HEIGHT below)
 *   --receipt  where the goodbye receipt goes (default: none written)
 *   --hair, --hair-color  her hair style and colour (creation choices,
 *              from 0; default style 3, an updo that leaves a tabard's back
 *              in view, colour 0)
 *   --race, --sex, --class  who wears the items (ChrRaces id, 0 male or
 *              1 female, ChrClasses id for the starting outfit worn under
 *              them); the client's night elf woman, a fresh hunter, by
 *              default
 *
 * Exits 0 when every line made all three pictures; 1 when any line was
 * refused (each named on the terminal and counted in the receipt): a page
 * missing a picture should be noticed, not filled with a stand-in.
 */
#include "013-errors.h"
#include "015-mpq.h"
#include "017-blp.h"
#include "025-character.h"
#include "030-moves.h"
#include "036-texture.h"
#include "041-figure.h"
#include "074-tables.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* {{{ the pictures' size and the camera
 * The worn pictures are taller than wide: a standing character. The
 * camera looks level at her middle from far enough that her whole height
 * (2.29 yards for the night elf woman, the tallest default) fits with a
 * margin; a narrow lens (30 degrees) keeps her proportions close to a
 * drawing's. Characters face +X in their own model space (every M2
 * character does), so the front view looks back along -X. */
#define WIDTH       512
#define HEIGHT      640
#define LENS_DEG    30.0f
#define LOOK_HEIGHT 1.15f                       /* yards: where the camera aims, about her waist */
#define DISTANCE    5.2f                        /* yards from her */
/* }}} */

/* {{{ the light
 * One fixed light, from in front, above and a little to her right, so
 * the tabard's front is lit and its folds show; the back view turns her,
 * not the light, so her back is lit the same way. Colours as a clear
 * midday's in Ashenvale, near enough: sun 0.85, shade 0.45 (the shade is
 * the light every surface gets; the sun adds by how squarely it faces
 * it). Fog is pushed out of reach: nothing here is far away. */
static const float SUN_DIR[3]   = { 0.62f, -0.30f, 0.72f };
static const float SUN_COL[3]   = { 0.85f, 0.83f, 0.78f };
static const float SHADE_COL[3] = { 0.45f, 0.45f, 0.48f };
static const float FOG_COL[3]   = { 0, 0, 0 };
static const float FOG_RANGE[2] = { 100000.0f, 100001.0f };
/* }}} */

/* {{{ the room (the handle: docs/001, "Handles"): what the program holds
 * from start to goodbye */
typedef struct {
    MpqChain      chain;
    Textures      textures;
    Tables       *tables;                           /* the archives' tables (074-tables.h; no resolver here) */
    Figures      *figures;
    Shader        shader;
    RenderTexture2D canvas;
    int           race, sex, class_id;
    int           hair_style, hair_color;           /* character-creation choices, from 0 (--hair, --hair-color) */
    const char   *out;
    int           lines, icons, worn, refused;      /* the receipt's counts */
} Room;
/* }}} */

/* {{{ static save_icon: the item's icon, decoded, as a PNG */
static WErr save_icon(Room *room, const char *name, int look, WWhy *why)
{
    char icon[300], path[1200];
    if (character_item_icon(NULL, &room->chain, look, icon, sizeof icon, why)) return why->code;
    size_t len;
    unsigned char *b = mpq_chain_read(&room->chain, icon, &len, NULL);
    if (!b) return w_fail(why, W_ERR_NOT_FOUND, "the icon %s is not in the client's archives", icon);
    BlpImage img;
    WWhy w2;
    int rc = blp_decode(b, len, &img, &w2);
    free(b);
    if (rc) return w_fail(why, w2.code, "%s: %s", icon, w2.text);
    Image im = { .data = img.rgba, .width = img.width, .height = img.height, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    snprintf(path, sizeof path, "%s/%s-icon.png", room->out, name);
    int ok = ExportImage(im, path);
    blp_free(&img);
    if (!ok) return w_fail(why, W_ERR_IO, "could not write %s", path);
    return W_OK;
}
/* }}} */

/* {{{ static save_view: the dressed figure from one side, as a PNG
 * Drawn into the room's canvas (a picture on the card with its own depth
 * buffer), cleared see-through first, so the page behind shows round
 * her. The card holds pictures bottom row first; it is turned upright
 * before saving. */
static WErr save_view(Room *room, const Figure *fig, const char *name, const char *side, float turn, WWhy *why)
{
    Vector3 eye = { DISTANCE, 0, LOOK_HEIGHT };
    Camera3D cam = { .position = eye, .target = { 0, 0, LOOK_HEIGHT }, .up = { 0, 0, 1 }, .fovy = LENS_DEG, .projection = CAMERA_PERSPECTIVE };
    SetShaderValue(room->shader, GetShaderLocation(room->shader, "eye"), &eye, SHADER_UNIFORM_VEC3);
    BeginTextureMode(room->canvas);
    ClearBackground((Color){ 0, 0, 0, 0 });
    BeginMode3D(cam);
    rlDisableBackfaceCulling();                     /* as the client: some pieces are one-sided sheets seen from both sides */
    figure_draw(fig, room->shader, MatrixRotateZ(turn));
    EndMode3D();
    EndTextureMode();
    Image im = LoadImageFromTexture(room->canvas.texture);
    ImageFlipVertical(&im);
    char path[1200];
    snprintf(path, sizeof path, "%s/%s-%s.png", room->out, name, side);
    int ok = ExportImage(im, path);
    UnloadImage(im);
    if (!ok) return w_fail(why, W_ERR_IO, "could not write %s", path);
    return W_OK;
}
/* }}} */

/* {{{ static save_worn: her, dressed in the item over her starting
 * outfit, standing at the first moment of her standing animation */
static WErr save_worn(Room *room, const char *name, const int items[SLOT_COUNT], WWhy *why)
{
    CharacterLook look = { room->race, room->sex, 0, 0, room->hair_style, room->hair_color, { 0 }, 0, 0 };
    if (character_start_outfit(NULL, &room->chain, room->race, room->class_id, room->sex, look.items, why)) return why->code;
    for (int k = 0; k < SLOT_COUNT; ++k) if (items[k]) look.items[k] = items[k];
    Figure fig;
    if (figure_from_character(room->figures, &fig, &look, why)) return why->code;
    /* drawn whole: pictured; anything of the outfit missing: refused, so no page shows a tabard it doesn't have */
    if (fig.items_missing) {
        int missing = fig.items_missing;
        figure_free(&fig);
        return w_fail(why, W_ERR_NOT_FOUND, "%d of the outfit's pictures or attached items are not in the client's archives (named above)", missing);
    }
    MovesState moves;
    moves_init(&moves, &fig.anim);                  /* standing */
    figure_bend(&fig, &moves, 0);
    /* front: as she was made, facing the camera; back: turned half round */
    WErr rc = save_view(room, &fig, name, "front", 0, why);
    if (!rc) rc = save_view(room, &fig, name, "back", PI, why);
    figure_free(&fig);
    return rc;
}
/* }}} */

/* {{{ static name_ok: a name safe to put in a file's name */
static int name_ok(const char *s)
{
    if (!s[0]) return 0;
    for (; *s; ++s) if (!isalnum((unsigned char)*s) && *s != '-' && *s != '_') return 0;
    return 1;
}
/* }}} */

/* {{{ static do_line: one list line's three pictures; a refusal is named
 * and counted, and the next line goes on */
static void do_line(Room *room, char *line, int number)
{
    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';
    char *s = line;
    while (*s == ' ' || *s == '\t') ++s;
    /* a blank line or a # line: nothing to make; anything else is one picture's line */
    if (!*s || *s == '#') return;
    ++room->lines;
    char name[200] = "", wear[400] = "", extra[8] = "";
    int fields = sscanf(s, "%199s %399s %7s", name, wear, extra);
    int items[SLOT_COUNT] = { 0 };
    char bad[200];
    /* exactly a name and one slot:look: made; else refused with what was wrong */
    if (fields != 2) { fprintf(stderr, "dressing room: line %d: \"%s\" isn't NAME SLOT:LOOK; refused\n", number, s); ++room->refused; return; }
    if (!name_ok(name)) { fprintf(stderr, "dressing room: line %d: the name \"%s\" may hold only letters, digits, - and _; refused\n", number, name); ++room->refused; return; }
    if (strchr(wear, ',') || character_wear_text(items, wear, bad, sizeof bad)) {
        fprintf(stderr, "dressing room: line %d: \"%s\" isn't one slot:look (slots: shirt legs feet wrists chest hands tabard waist head shoulders back mainhand offhand); refused\n", number, wear);
        ++room->refused;
        return;
    }
    int look = 0;
    for (int k = 0; k < SLOT_COUNT; ++k) if (items[k]) look = items[k];
    WWhy why;
    int failed = 0;
    if (save_icon(room, name, look, &why)) { fprintf(stderr, "dressing room: line %d (%s): no icon: %s\n", number, name, why.text); failed = 1; }
    else ++room->icons;
    if (save_worn(room, name, items, &why)) { fprintf(stderr, "dressing room: line %d (%s): not drawn worn: %s\n", number, name, why.text); failed = 1; }
    else room->worn += 2;
    room->refused += failed;
}
/* }}} */

/* {{{ static goodbye: the receipt, written last: its presence says the
 * run reached its end, its counts say what it made */
static void goodbye(const Room *room, const char *receipt, const char *list, double seconds)
{
    if (!receipt) return;
    FILE *f = fopen(receipt, "w");
    if (!f) { fprintf(stderr, "dressing room: could not write the receipt %s\n", receipt); return; }
    char when[64];
    time_t now = time(NULL);
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "goodbye from the dressing room (scripts/item-pictures, issue 410)\n");
    fprintf(f, "ended:        %s\n", when);
    fprintf(f, "seed:         none (nothing in a run is random)\n");
    fprintf(f, "list:         %s\n", list);
    fprintf(f, "pictures in:  %s\n", room->out);
    fprintf(f, "wearer:       race %d, sex %d, class %d's starting outfit, hair style %d colour %d\n", room->race, room->sex, room->class_id, room->hair_style, room->hair_color);
    fprintf(f, "lines:        %d\n", room->lines);
    fprintf(f, "icons:        %d\n", room->icons);
    fprintf(f, "worn views:   %d (front and back)\n", room->worn);
    fprintf(f, "refused:      %d\n", room->refused);
    fprintf(f, "seconds:      %.1f\n", seconds);
    fclose(f);
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv)
{
    static Room room;
    const char *client = NULL, *list = NULL, *receipt = NULL;
    room.race = 4; room.sex = 1; room.class_id = 3;     /* the client's own: a night elf woman, a fresh hunter */
    /* her hair up (the owner, 2026-10-23: "pick a model with an updo"): the
     * client's own long hair (style 0) hides a tabard's back; of the night
     * elf woman's styles, 3 is swept up off the neck and leaves the tabard
     * whole from both sides (6, the other updo, lays a braid over the
     * front crest) */
    room.hair_style = 3;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--client") && i + 1 < argc) client = argv[++i];
        else if (!strcmp(argv[i], "--list") && i + 1 < argc) list = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) room.out = argv[++i];
        else if (!strcmp(argv[i], "--receipt") && i + 1 < argc) receipt = argv[++i];
        else if (!strcmp(argv[i], "--race") && i + 1 < argc) room.race = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sex") && i + 1 < argc) room.sex = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--class") && i + 1 < argc) room.class_id = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hair") && i + 1 < argc) room.hair_style = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hair-color") && i + 1 < argc) room.hair_color = atoi(argv[++i]);
        else { fprintf(stderr, "dressing room: \"%s\" isn't an option\n", argv[i]); return 2; }
    }
    if (!client || !list || !room.out) { fprintf(stderr, "usage: dressing-room --client DIR --list FILE --out DIR [--receipt FILE] [--race ID] [--sex 0|1] [--class ID] [--hair N] [--hair-color N]\n"); return 2; }
    FILE *lf = fopen(list, "r");
    if (!lf) { fprintf(stderr, "dressing room: can't read the list %s\n", list); return 1; }
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* the window only lends its graphics card context: hidden, small */
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "dressing room");
    WWhy why;
    if (mpq_chain_open(&room.chain, client, "enUS", &why)) { fprintf(stderr, "%s\n", why.text); CloseWindow(); return 1; }
    textures_init(&room.textures, 5);                /* the sharpest filtering the card offers: nothing here is in a hurry */
    room.tables = tables_new(NULL, &room.chain);
    room.figures = figures_new(&room.chain, NULL, room.tables, &room.textures);
    room.shader = figure_shader();
    SetShaderValue(room.shader, GetShaderLocation(room.shader, "sun_dir"), SUN_DIR, SHADER_UNIFORM_VEC3);
    SetShaderValue(room.shader, GetShaderLocation(room.shader, "sun_col"), SUN_COL, SHADER_UNIFORM_VEC3);
    SetShaderValue(room.shader, GetShaderLocation(room.shader, "shade_col"), SHADE_COL, SHADER_UNIFORM_VEC3);
    SetShaderValue(room.shader, GetShaderLocation(room.shader, "fog_col"), FOG_COL, SHADER_UNIFORM_VEC3);
    SetShaderValue(room.shader, GetShaderLocation(room.shader, "fog_range"), FOG_RANGE, SHADER_UNIFORM_VEC2);
    room.canvas = LoadRenderTexture(WIDTH, HEIGHT);

    char line[1000];
    for (int n = 1; fgets(line, sizeof line, lf); ++n) do_line(&room, line, n);
    fclose(lf);

    UnloadRenderTexture(room.canvas);
    UnloadShader(room.shader);
    figures_free(room.figures);
    tables_free(room.tables);
    mpq_chain_close(&room.chain);
    CloseWindow();
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double seconds = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    printf("dressing room: %d lines, %d icons, %d worn views, %d refused, %.1f seconds; pictures in %s\n",
           room.lines, room.icons, room.worn, room.refused, seconds, room.out);
    goodbye(&room, receipt, list, seconds);
    return room.refused ? 1 : 0;
}
/* }}} */
