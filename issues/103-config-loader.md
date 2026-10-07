# 103 — Config Loader

*Filled in part by the code from wow-chat-2026 (its 173b1 (the settings store)), 2026-10-07.*

## Current behavior

Built, differently from the plan below: `src/014-settings.c`, a store
of every setting declared once (the original's names, kinds, ranges,
defaults, sections, whether a restart is needed), with the player's
`Config.wtf` in the original's `SET name "value"` form (bad lines named
by number and skipped), read by name each time it is needed. A handle:
several can live at once (a profile per server). Paths and the start
come from the run script's arguments (`scripts/run`: `--maps`,
`--client`, `--world`, `--settings`), not an `input/client.ini` yet.

## Intended behavior

Per CLAUDE.md ("the first thing a program should do is read the input/
files. from there, it can know exactly how to start up.") the client's
first action after argv parsing is to read its configuration from
`${DIR}/input/` plus an optional `--config <path>` override.

Two file kinds are loaded:

- **`input/client.ini`** — key/value pairs grouped by `[section]`. Things
  like `[paths] asset_root = /path/to/MPQs`, `[render] vsync = true`,
  `[net] auth_host = 127.0.0.1`. W client additions (2026-09-23):
  `[paths] packs = …` (replacement pack folders, highest priority first),
  `[paths] overlays = …` (loose folders such as converted WC3 maps),
  `[paths] extra_archives = …` (custom patch archives appended to the chain),
  `[assets] strict = true|false` (placeholder use is an error when true),
  `[mode] start = wow|wc3`.
- **`input/realms.ini`** — saved realm endpoints (the original client
  caches the last-used realm).

The loader produces a typed `config_t` struct. Every field has a default
in C so a missing key is not a fallback — it is the explicit default,
logged at `info` severity at startup.

Missing files, malformed entries, or unknown keys are errors per the
CLAUDE.md "warnings as errors" rule. The loader prints what failed and
exits non-zero.

## Suggested implementation steps

1. Create `src/config/` subsystem.
2. Define `config_t` in `config.h`. Group fields by `paths`, `render`,
   `net`, `log`, `audio`.
3. Implement a small INI parser. No quoted strings, no continuations,
   `#` and `;` for comments, UTF-8 only.
4. Implement `config_load(const char *path, config_t *out)`.
5. Implement `config_log(const config_t *)` that dumps the effective
   config at `info` severity.
6. Create `input/client.ini.example` with all fields documented.
7. Tests: known-good file parses correctly, malformed file errors
   cleanly, unknown key errors cleanly, missing required field errors
   cleanly.

## Acceptance

- `custom-client --config tests/fixtures/known-good.ini` runs to the
  next-phase stub and prints the loaded config.
- Each error case exits non-zero with one log line that names the file,
  line number, and what was wrong.

## Related

- Blocked by: [101](./101-build-system.md), [102](./102-logging-and-errors.md).
- Blocks: [104](./104-fs-abstraction.md).
