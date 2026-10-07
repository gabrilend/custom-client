/*
 * 014-settings.h (issue 173b1) - the settings store
 *
 * For a general audience: every setting the client has (window size,
 * camera speeds, and later every panel of the Options menu) is declared
 * once, in one table in 014-settings.c, with its kind, range and default. The
 * player's changes are kept in one text file shaped like the original
 * game's WTF/Config.wtf (SET name "value"), holding only what differs
 * from the defaults. Code reads a setting by name each time it uses it,
 * so a change takes effect at once, with nothing to rebuild.
 *
 * Names are the original game's own (its "CVars": cameraYawMoveSpeed,
 * gxResolution ...) where the setting is the same thing, so the reference
 * page docs/custom-client-stock-options.md doubles as the list of names to
 * use. A project setting with no original twin gets a name of its own.
 *
 * A Setting, field by field:
 *   decl  the declaration (a SettingDecl in 014-settings.c's table, read-only)
 *   num   double: the value as a number (toggle 0/1, slider, a numeric
 *         choice); 0 for text
 *   text  char[64]: the value as written in the file ("1280x720", "180")
 */
#ifndef EG_SETTINGS_H
#define EG_SETTINGS_H

#include "013-errors.h"

#include <stdio.h>

typedef enum {
    SETTING_TOGGLE,                   /* on/off: "0" or "1" */
    SETTING_SLIDER,                   /* a number from min to max */
    SETTING_CHOICE,                   /* one of `choices`' values */
    SETTING_TEXT,                     /* typed in, held to `check` */
} SettingKind;

typedef struct {
    const char *label;                /* shown in the menu */
    const char *value;                /* written in the file */
} SettingChoice;

typedef struct {
    const char *name;                 /* the stored name */
    const char *section;              /* the menu panel it belongs to */
    SettingKind kind;
    const char *def;                  /* the default, as written in the file */
    double min, max, step;            /* sliders only */
    const SettingChoice *choices;     /* choices only, ended by a { NULL, NULL } */
    const char *(*check)(const char *value);   /* text only: NULL if fine, else why not */
    int restart;                      /* 1: read once at start; 0: read live */
    const char *label, *explain;      /* the menu's words and tooltip */
} SettingDecl;

typedef struct {
    const SettingDecl *decl;
    double num;
    char text[64];
} Setting;

/* A store of settings (the handle: docs/001, "Handles"), every setting at
 * its default; freed with settings_free. More than one may live at once
 * (a profile per server, a test's own). */
typedef struct Settings Settings;
Settings *settings_new(void);
void      settings_free(Settings *st);

/* The setting by name, or NULL when there is none. */
const Setting *settings_find(const Settings *st, const char *name);

/* How many settings, and the i-th in declaration order (for the menu). */
int settings_count(void);
const Setting *settings_at(const Settings *st, int i);

/* Changes a setting. W_OK; otherwise W_ERR_REFUSED: the value is refused, the
 * setting is unchanged, and `why` says why (unknown name, out of range,
 * not one of the choices, not a resolution ...). */
WErr settings_set(Settings *st, const char *name, const char *value, WWhy *why);

/* Reads the player's file. Each line that names an unknown setting,
 * holds a refused value, or isn't `SET name "value"` is written to
 * `report` with its line number and skipped. Blank lines and lines
 * starting with # are skipped quietly. A missing file is reported once
 * and leaves every setting at its default. Returns the number of lines
 * skipped for a problem. */
int settings_load(Settings *st, const char *path, FILE *report);

/* Writes every setting that differs from its default to `path`. W_OK, or
 * W_ERR_IO with `why`. */
WErr settings_save(const Settings *st, const char *path, WWhy *why);

/* The value of a setting, by name. An unknown name is a mistake in the
 * code, not in the player's file: it stops the client with the name and
 * the file and line asking for it. */
double settings_num_at(const Settings *st, const char *name, const char *file, int line);
const char *settings_text_at(const Settings *st, const char *name, const char *file, int line);
#define settings_num(st, name)  settings_num_at((st), (name), __FILE__, __LINE__)
#define settings_text(st, name) settings_text_at((st), (name), __FILE__, __LINE__)

/* gxResolution's value split into width and height; 0 on success. */
int settings_resolution(const char *value, int *width, int *height);

#endif
