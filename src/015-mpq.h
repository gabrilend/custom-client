/*
 * 015-mpq.h (issue 173k; the C twin of lib/mpq.lua) - files out of the game's archives
 *
 * For a general audience: the original game keeps every model, texture
 * and table inside a few large archive files (the .MPQ files in Data). Each is a small
 * filing cabinet: an index (the "hash table") says, for a file's name,
 * which drawer it is in; a second list (the "block table") says where that
 * drawer starts, how big it is, and whether its contents are compressed or
 * scrambled; both lists are scrambled with a fixed, publicly known recipe.
 * This reads them, finds a file by name, and unpacks it (zlib, sector by
 * sector). A chain searches the client's archives newest patch first.
 *
 * MPQ versions 1 and 2 (the 3.3.5a client's), from the format's public
 * description; only what these archives use: zlib, file keys with or
 * without the position fix. Anything else is an error naming the file.
 */
#ifndef EG_MPQ_H
#define EG_MPQ_H

#include "013-errors.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char      path[600];
    FILE     *file;
    uint64_t  base;               /* where the archive starts in the file */
    uint32_t  sector;             /* bytes a sector */
    uint32_t *hashes, nhash;      /* 4 words an entry: name A, name B, locale/platform, block */
    uint32_t *blocks, nblock;     /* 4 words an entry: position, packed size, size, flags */
} MpqArchive;

/* A chain also remembers files it has unpacked (173t): many figures read
 * the same tables, models and animations, and unpacking is what costs.
 * Up to MPQ_KEEP_BYTES of unpacked files are kept; past that, the one
 * read longest ago is forgotten. Readers still get their own copy. */
#define MPQ_KEEP_BYTES ((size_t)256 << 20)
typedef struct { char name[260]; unsigned char *data; size_t len; uint64_t used; const char *from; } MpqKept;

typedef struct {
    MpqArchive *archives[16];
    int         count;
    MpqKept    *kept;  int nkept, kept_cap;
    size_t      kept_bytes;
    uint64_t    tick;                 /* counts reads, for "read longest ago" */
    uint64_t    hits, misses;         /* for reports */
    double      unpack_s;             /* seconds spent unpacking files the memory didn't hold (for reports) */
    pthread_mutex_t lock;             /* one reader at a time: the read-ahead thread (034-prefetch.h) shares the chain (173u) */
} MpqChain;

uint32_t mpq_hash(const char *text, int kind);
/* W_OK, or a code with `why` (013-errors.h). */
WErr      mpq_open(MpqArchive *a, const char *path, WWhy *why);
/* A malloc'd copy of `name` and its size, or NULL when the archive has
 * none; on a format it can't read, NULL with `why` filled. */
unsigned char *mpq_read(MpqArchive *a, const char *name, size_t *size, WWhy *why);
void     mpq_close(MpqArchive *a);

/* The client's archives under `data` (its Data folder) for `locale` ("enUS"). */
WErr      mpq_chain_open(MpqChain *c, const char *data, const char *locale, WWhy *why);
/* The newest archive's copy, its size, and which archive (may be NULL).
 * Served from the chain's memory when it was unpacked lately. Safe to
 * call from more than one thread (the chain is locked while it reads). */
unsigned char *mpq_chain_read(MpqChain *c, const char *name, size_t *size, const char **from);
void     mpq_chain_close(MpqChain *c);

#endif
