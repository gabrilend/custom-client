# 014-settings

*Written by `scripts/generate-info` from `014-settings.h`; don't edit, re-run it.*

014-settings.h (issue 173b1) - the settings store

For a general audience: every setting the client has (window size,
camera speeds, and later every panel of the Options menu) is declared
once, in one table in 014-settings.c, with its kind, range and default. The
player's changes are kept in one text file shaped like the original
game's WTF/Config.wtf (SET name "value"), holding only what differs
from the defaults. Code reads a setting by name each time it uses it,
so a change takes effect at once, with nothing to rebuild.

Names are the original game's own (its "CVars": cameraYawMoveSpeed,
gxResolution ...) where the setting is the same thing, so the reference
page docs/custom-client-stock-options.md doubles as the list of names to
use. A project setting with no original twin gets a name of its own.

A Setting, field by field:
  decl  the declaration (a SettingDecl in 014-settings.c's table, read-only)
  num   double: the value as a number (toggle 0/1, slider, a numeric
        choice); 0 for text
  text  char[64]: the value as written in the file ("1280x720", "180")

## What it offers

- `type SettingKind`
- `type SettingChoice`
- `type SettingDecl`
- `type Setting`
- `void settings_init(void);`
  Puts every setting at its default. Called once at start, before load.
- `const Setting *settings_find(const char *name);`
  The setting by name, or NULL when there is none.
- `int settings_count(void);`
  How many settings, and the i-th in declaration order (for the menu).
- `const Setting *settings_at(int i);`
- `int settings_set(const char *name, const char *value, char *why, int why_len);`
  Changes a setting. 0 on success; otherwise the value is refused, the
  setting is unchanged, and `why` says why (unknown name, out of range,
  not one of the choices, not a resolution ...).
- `int settings_load(const char *path, FILE *report);`
  Reads the player's file. Each line that names an unknown setting,
  holds a refused value, or isn't `SET name "value"` is written to
  `report` with its line number and skipped. Blank lines and lines
  starting with # are skipped quietly. A missing file is reported once
  and leaves every setting at its default. Returns the number of lines
  skipped for a problem.
- `int settings_save(const char *path, char *why, int why_len);`
  Writes every setting that differs from its default to `path`. 0 on
  success; otherwise `why` says why.
- `double settings_num_at(const char *name, const char *file, int line);`
  The value of a setting, by name. An unknown name is a mistake in the
  code, not in the player's file: it stops the client with the name and
  the file and line asking for it.
- `const char *settings_text_at(const char *name, const char *file, int line);`
- `int settings_resolution(const char *value, int *width, int *height);`
  gxResolution's value split into width and height; 0 on success.
