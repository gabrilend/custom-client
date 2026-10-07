# 013-errors

*Written by `scripts/generate-info` from `013-errors.h`; don't edit, re-run it.*

013-errors.h - what went wrong: a code to branch on, a sentence to read

For a general audience: when something can't be done (a file isn't in
the archives, a file's bytes don't make sense, a setting is out of its
range), the part that found out says so twice: once as a code from the
fixed list below, which the calling code can act on ("not there: try
the next place"; "broken: stop"), and once as a sentence for a person,
naming exactly what failed and where. The owner, 2026-10-07: "The W
client's errors feel more flexible because they allow the calling code
to branch depending on errors. Ours is more readable, but we could just
create more verbose error types and expand the list." So the list grows
whenever a caller needs to tell two failures apart
(docs/001-architecture-overview.md, "Errors").

A WWhy, field by field:
  code  WErr: the kind of failure, W_OK when nothing failed
  text  char[600]: the sentence, "" when nothing failed

How a part reports: a call that can fail takes a WWhy * as its last
argument. On failure it fills it with w_fail and returns the same code
(a call that returns a pointer returns NULL instead, and the code is in
the WWhy). On success it returns W_OK and leaves the WWhy alone.

## What it offers

- `type WErr`
- `type WWhy`
- `WErr w_fail(WWhy *why, WErr code, const char *format, ...) __attribute__((format(printf, 3, 4)));`
  Fills `why` with `code` and the sentence (printf's format), and returns
  `code`, so a failure is one line: return w_fail(why, W_ERR_NOT_FOUND, ...).
- `const char *w_err_name(WErr code);`
  The code's name, for reports ("W_ERR_NOT_FOUND").
