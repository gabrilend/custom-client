/*
 * 068-goodbye.h (issue 1109) - the goodbye receipt the editor reads back
 *
 * For a general audience: when the map editor's Test Map opens this
 * client (067-test-map.h), it waits for the client to end and then reads
 * a small file the client leaves behind, its goodbye receipt: which map
 * ran, when it started and ended, for how long, how many frames were
 * drawn, and why it ended. No receipt at all means the client crashed,
 * and the editor says so. So the receipt is written last, and all at
 * once: into a temporary file in the same folder, then renamed onto the
 * receipt's name, so the editor never reads half a receipt, and an
 * earlier run's receipt is replaced whole or not at all.
 *
 * The receipt is a Lua table of plain data: world-edit-to-execute reads
 * it with LuaJIT (loadfile, with an empty environment), and it holds
 * only what the client's own data-only reader takes (059-loose-folders.h:
 * strings with Lua's escapes, numbers, one table, a comment). Format 1:
 *
 *   -- the W client's goodbye receipt (issue 1109)
 *   return {
 *       format  = 1,
 *       map     = "/home/me/maps/daow-build",  -- the --test-map folder, as given
 *       started = "2026-11-04T20:31:07Z",      -- when the client started, UTC
 *       ended   = "2026-11-04T20:33:52Z",      -- when it wrote this, UTC
 *       seconds = 165.204,                     -- between the two, by the steady clock
 *       frames  = 9873,                        -- frames drawn
 *       reason  = "closed",                    -- closed, lost-connection or error
 *       why     = "...",                       -- only with error: the sentence
 *       steps   = 640,                         -- a run's own counts, when it gives any
 *   }
 *
 *   closed           the player closed the window or pressed Escape
 *   lost-connection  the server went away (Phase 5's 609; nothing ends so yet)
 *   error            the client stopped on something it couldn't do, said
 *                    in `why` (a missing password, a map folder not there ...)
 *
 * After those, a run may add its own counts, each a name and a number
 * (goodbye_count): a headless run (089-headless-run.h) gives how many
 * steps it moved, where she ended, which looks it found ... A reader that
 * doesn't know a count passes it over, so the format stays 1.
 *
 * Seconds come from the steady clock, so a change of the machine's
 * clock during the run doesn't change them; started and ended are the
 * machine's clock in UTC, to the second.
 *
 * Window-free: in the test program and the executable.
 */
#ifndef EG_GOODBYE_H
#define EG_GOODBYE_H

#include "013-errors.h"

#include <stdint.h>
#include <time.h>

typedef enum { GOODBYE_CLOSED, GOODBYE_LOST_CONNECTION, GOODBYE_ERROR, GOODBYE_REASON_COUNT } GoodbyeReason;

/* The reason as the receipt writes it ("closed", "lost-connection", "error"). */
const char *goodbye_reason_name(GoodbyeReason reason);

/* A run that ends with a receipt (the handle: docs/001, "Handles"). Its
 * start is now. `receipt` is the file to write at the end, and `map` the
 * map's folder as given (both copied). The receipt's folder must be there
 * now, so a run whose receipt could never be written stops at its start:
 * W_ERR_NOT_FOUND when it isn't a folder, W_ERR_REFUSED for an empty or
 * too long path. */
typedef struct Goodbye Goodbye;
WErr goodbye_begin(const char *receipt, const char *map, Goodbye **out, WWhy *why);
void goodbye_free(Goodbye *g);

/* A frame drawn. Nothing with a NULL handle (a run with no receipt). */
void goodbye_frame(Goodbye *g);

/* Writes the receipt, once: `reason`, and with GOODBYE_ERROR the sentence
 * `why_text` (NULL or "" for the others; error with none says so). W_OK;
 * W_ERR_IO when the temporary file can't be made, written or renamed (it
 * is removed again; an earlier receipt stays as it was); W_ERR_REFUSED for
 * a second call or a reason outside the list. W_OK and nothing written
 * with a NULL handle. */
WErr goodbye_end(Goodbye *g, GoodbyeReason reason, const char *why_text, WWhy *why);

/* A count of the run's own for the receipt, written after its fields as
 * `name = value` (the value exactly: %.17g). `name`: small letters,
 * digits and _, a letter first, at most GOODBYE_NAME_MAX characters, and
 * not one of the receipt's own fields. A name given again keeps its place
 * and takes the new value. W_OK; W_ERR_REFUSED for a name as above, a
 * value that isn't finite, or a receipt already written; W_ERR_FULL past
 * GOODBYE_COUNTS_MAX names. W_OK and nothing kept with a NULL handle. */
#define GOODBYE_NAME_MAX   31
#define GOODBYE_COUNTS_MAX 48
WErr goodbye_count(Goodbye *g, const char *name, double value, WWhy *why);

/* `t` as the receipt writes it, "2026-11-04T20:31:07Z" (20 characters and the end). */
void goodbye_utc(time_t t, char out[21]);

#endif
