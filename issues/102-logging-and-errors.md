# 102 — Logging and Errors

*Filled in part by the code from wow-chat-2026 (its 173v (answer 13)), 2026-10-07.*

## Current behavior

Built (2026-10-07): the error half. `src/013-errors.h`: a `WErr` code
from one list (not found, corrupt, unsupported, refused, full,
input/output, graphics card) and a `WWhy` carrying the code and a
sentence naming exactly what failed; every part that can fail returns
the code and fills the sentence; failures passed on keep their code.
Tests check the codes.

Not built: the logging half (levels, subsystems, a log file in the RAM
tier). Messages go to the terminal with `fprintf` today, warnings marked
by their wording.

## Intended behavior

A small logging API that every subsystem uses:

```c
log_trace(SUBSYS_MPQ, "opened archive: %s", path);
log_warn (SUBSYS_NET, "session re-key in %dms", ms);
log_error(SUBSYS_GFX, "shader compile failed: %s", info_log);
```

Output goes to stderr (colorized when a TTY) and to a rotating file in
`${DIR}/tmp/log/`. The `tmp/` directory is the RAM-backed symlink, so
disk does not slow us down.

A typed error model alongside:

```c
typedef enum { ERR_OK, ERR_IO, ERR_PARSE, ERR_PROTO, ERR_GL, ERR_OOM, ... } err_t;
```

Functions that can fail return `err_t`, with output via pointer args. No
sentinel return values, no `errno` smuggling.

Per CLAUDE.md: fallbacks are warnings, warnings are errors. The default
log filter for development is `trace` and up; release builds default to
`info`.

## Suggested implementation steps

1. Create `src/log/` subsystem. Files: `log.h`, `log.c`, `log_color.c`,
   tests under `tests/log/`.
2. Define subsystem enum (start with: `LOG`, `CFG`, `FS`, `MPQ`, `DBC`,
   `BLP`, `GFX`, `NET`, `UI`, `LUA`, `AUDIO`, `GAME`).
3. Implement the printf-family loggers as macros that capture `__FILE__`,
   `__LINE__`, `__func__` and forward to a single sink function.
4. Implement the file sink: open `${DIR}/tmp/log/client-<pid>.log` at
   init, write timestamped lines, rotate at 16 MiB.
5. Implement the stderr sink: ANSI color per severity when `isatty(2)`.
6. Define `err_t` and a `const char *err_str(err_t)` helper.
7. Tests: a handful covering severity filtering, subsystem filtering,
   and that the log file is created in `tmp/`.

## Acceptance

- All future subsystems use `log_*` rather than `printf` / `fprintf`.
- A log line for a known-error case prints exactly once at the configured
  severity.
- Disabling a subsystem at runtime (env var `CC_LOG_SUBSYS=-net`) drops
  its trace/debug lines and keeps its warn/error.

## Related

- Blocked by: [101](./101-build-system.md).
- Blocks: every other Phase 1 issue.
