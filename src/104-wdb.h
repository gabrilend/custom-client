/*
 * 104-wdb.h (issue 509) - the server's answers, kept on disk between runs
 *
 * For a general audience: the server tells the client about creatures,
 * items, quests and names one record at a time, when asked. The client
 * keeps each answer, so it asks once, and keeps them in files between runs
 * in a folder called WDB, so the next run asks even less. This is that
 * keeping: one cache per kind of record, the files they are kept in, and
 * the limits on how fast two kinds may be asked for.
 *
 * The file (issue 509): a header of six little-endian 32-bit numbers
 * (signature, build, locale, record size, format version, cache version),
 * then entries of key, length and the record's bytes, ended by a zero key
 * with zero length. A file whose first five numbers are not this cache's
 * is ignored whole, and so is one that stops before its end; the sixth,
 * the cache version, is the server's, and a new one from the server
 * empties the cache (wdb_server_version).
 *
 * What a kind is (its signature, its record size, whether it is kept on
 * disk, its format version and its query limit) is given by the caller as
 * a WdbKind. The kinds' own values wait on docs/057's note 10.1, except
 * the few the issue states (the WDB_* numbers below). A kind with no
 * signature yet is kept in memory only, so no file is ever written that
 * the original could misread.
 *
 * Asking is the caller's: the cache calls the WdbAsk it was given when a
 * record must be asked for, and the caller sends the query. Answers come
 * back through wdb_answer, one record each; a reply carrying several
 * (it starts with a one-byte count) is split by the packet code, which
 * knows each record's layout. Time is the caller's too: the limits count
 * in the seconds it passes to wdb_want and wdb_tick.
 */
#ifndef W_WDB_H
#define W_WDB_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

#define WDB_BUILD           12340u        /* the client build the files are for */
#define WDB_NO_SUCH_RECORD  0x80000000u   /* a reply id with this bit set: no such record */
#define WDB_WINDOW_SECONDS  30.0          /* the query limits' window */
#define WDB_ITEM_LIMIT      256           /* item queries a window */
#define WDB_NAME_LIMIT      128           /* name queries a window */
#define WDB_FORMAT_ITEM     5u            /* format versions the issue states; the rest are 1 */
#define WDB_FORMAT_QUEST    3u
#define WDB_FORMAT_NAME     4u
#define WDB_FORMAT_OTHER    1u
#define WDB_RECORD_MAX      (1u << 20)    /* a record longer than this is refused, read or answered */

typedef struct {
    const char *name;             /* for sentences: "items" */
    const char *file;             /* its file in the WDB folder: "itemcache.wdb"; NULL for memory only */
    char        signature[4];     /* the header's first four bytes; all zero: not known yet, memory only */
    uint32_t    record_size;      /* the header's fourth number */
    uint32_t    format_version;   /* the header's fifth */
    int         limit;            /* queries in each 30-second window; 0 for no limit */
} WdbKind;

/* Called once for each query the cache sends, in the order it sends
 * them: ask the server for `key`. */
typedef void (*WdbAsk)(void *ctx, const WdbKind *kind, uint32_t key);
/* Called once for each waiter when its record's answer comes: `ok` 1 and
 * the record (borrowed until the call returns), or `ok` 0 with no record
 * when the server has no such record. */
typedef void (*WdbTell)(void *ctx, uint32_t key, uint64_t waiter, int ok, const unsigned char *rec, uint32_t len);

typedef struct WdbCache WdbCache;

/* An empty cache of one kind, for one locale (the header's third number),
 * asking through `ask` and telling waiters through `tell` (either NULL
 * for none). The kind must outlive the cache. NULL with `why` filled when
 * memory runs out or the kind is refused (a record size of 0 on a kind
 * kept on disk). */
WdbCache *wdb_new(const WdbKind *kind, uint32_t locale, WdbAsk ask, WdbTell tell, void *ctx, WWhy *why);
/* Frees the cache, writing nothing (wdb_close writes first). */
void      wdb_free(WdbCache *c);

/* The record kept for `key`: 1 and the record (borrowed until the cache
 * next changes) or 0 when none is kept. Asks nothing. */
int       wdb_find(const WdbCache *c, uint32_t key, const unsigned char **rec, uint32_t *len);
/* The record for `key`, wanted by `waiter`: 1 and the record when it is
 * kept, else 0, and the waiter is told when the answer comes. The first
 * want of a key asks for it, now if the window has room, else queued in
 * order for the next window; later wants of the same key wait on that one
 * query. W_ERR_FULL in `why` (and -1) when memory runs out. */
int       wdb_want(WdbCache *c, uint32_t key, uint64_t waiter, double now, const unsigned char **rec, uint32_t *len, WWhy *why);
/* Sends what the queue holds once a new window has begun, as many as the
 * limit allows. Answers how many were sent. */
int       wdb_tick(WdbCache *c, double now);
/* How many queries wait for a later window. */
int       wdb_queued(const WdbCache *c);

/* The server's answer for `id`: the record is kept (replacing any before)
 * and every waiter told, or, with WDB_NO_SUCH_RECORD in `id`, every waiter
 * of the key told it failed and nothing kept. W_ERR_REFUSED for a record
 * longer than WDB_RECORD_MAX, W_ERR_FULL when memory runs out. */
WErr      wdb_answer(WdbCache *c, uint32_t id, const unsigned char *rec, uint32_t len, WWhy *why);
/* The server's cache version: a different one from the cache's empties
 * it (answers 1); the same changes nothing (0). */
int       wdb_server_version(WdbCache *c, uint32_t version);
uint32_t  wdb_version(const WdbCache *c);
int       wdb_count(const WdbCache *c);

/* Loads the cache's file from `folder`. A file that isn't there, or whose
 * first five header numbers are not this cache's, or that stops before
 * its end, leaves the cache as it was and answers W_OK with `loaded` 0
 * (and the reason in `why`'s text, its code W_OK, for the log); a file
 * that cannot be read is W_ERR_IO. Entries load over any kept already.
 * A kind kept in memory only loads nothing. */
WErr      wdb_load(WdbCache *c, const char *folder, int *loaded, WWhy *why);
/* Writes the whole cache to its file in `folder`, through a file beside
 * it renamed over it, so a run stopped mid-write leaves the old file. A
 * kind kept in memory only writes nothing. */
WErr      wdb_save(const WdbCache *c, const char *folder, WWhy *why);
/* What a cache does at shutdown: writes (wdb_save), then frees. */
WErr      wdb_close(WdbCache *c, const char *folder, WWhy *why);

/* The WDB folder under `root`: `root`/Cache/WDB/`locale` when the game's
 * data is present, else `root`/WDB; made if missing, `root` too (but not its parents). */
WErr      wdb_folder(const char *root, int data_present, const char *locale, char *out, size_t n, WWhy *why);

#endif
