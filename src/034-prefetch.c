/*
 * 034-prefetch.c (issue 173u) - reading ahead of her, on a second thread; see 034-prefetch.h
 *
 * For a general audience: a queue of tiles, a thread that takes them one
 * at a time, and for each the list of files the tile will want, read
 * through the archive reader (which keeps them). Reading a file it
 * already holds costs a copy, so asking twice is cheap; each tile is
 * still asked only once (until she moves away and it is forgotten).
 * When told to (prefetch_prepare_ground), it also prepares each tile's
 * ground (033-ground-prep.h) and keeps it until the main thread takes it.
 */
#include "034-prefetch.h"
#include "013-errors.h"
#include "020-adt.h"
#include "033-ground-prep.h"
#include "022-m2.h"
#include "021-wmo.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ a reader-ahead (the handle: 034-prefetch.h): the queue (tile
 * numbers tx * 64 + ty), which tiles were ever asked, the thread, and the
 * prepared ground (173u): per tile, 0 none, 1 prepared (in `ready`), 2
 * the map has no ground file there; `skip`: the main thread loaded it
 * itself, so a queued preparation would be wasted; `working_on`: the tile
 * the thread is on, -1 none (a take waits for it). Everything below
 * `thread` is shared by both threads and read or written under `lock`
 * (`files_read` alone counts with atomic adds). */
struct Prefetch {
    MpqChain       *chain;
    char            map[64];
    int             started;          /* 0: reading ahead is off (asked so, or the thread couldn't start) */
    pthread_t       thread;
    pthread_mutex_t lock;
    pthread_cond_t  wake, idle;
    int             queue[64 * 64], head, tail;
    unsigned char   asked[64 * 64];
    int             busy, stopping;
    int             tiles_done, files_read, grounds_made, grounds_taken;
    const GroundPlace *place;
    int           (*on_card)(const char *name);
    GroundPrep     *ready[64 * 64];
    unsigned char   ground_state[64 * 64], skip[64 * 64];
    int             working_on;
};
/* }}} */

/* {{{ static touch: a file read through the chain (and so kept), then let go */
static unsigned char *touch(Prefetch *pf, const char *name, size_t *len)
{
    const char *from;
    unsigned char *s = mpq_chain_read(pf->chain, name, len, &from);
    if (s) __atomic_add_fetch(&pf->files_read, 1, __ATOMIC_RELAXED);
    return s;
}
/* }}} */

/* {{{ static read_tile: everything tile i's load will read, and its
 * ground prepared when asked for; `ground` gets the preparation (or
 * stays NULL) and `state` its kind (as pf->ground_state) */
static void read_tile(Prefetch *pf, int i, GroundPrep **ground, int *state)
{
    int tx = i / 64, ty = i % 64;
    char path[300];
    size_t len;
    WWhy why;
    AdtTile *t = NULL;
    *ground = NULL;
    *state = 0;
    pthread_mutex_lock(&pf->lock);
    int prepare = pf->place && !pf->skip[i];
    pthread_mutex_unlock(&pf->lock);
    if (prepare) {
        /* the ground prepared: its file read (and so kept) on the way */
        int got = ground_prep_tile(pf->place, tx, ty, pf->on_card, ground, &why);
        if (got == W_ERR_NOT_FOUND) { *state = 2; return; }   /* open sea: no ground file */
        if (got == W_OK) { *state = 1; t = (*ground)->adt; __atomic_add_fetch(&pf->files_read, 1, __ATOMIC_RELAXED); }
        /* any other code: the main thread prepares it again when it loads it, and says what is wrong */
    }
    if (!t) {
        snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", pf->map, pf->map, ty, tx);
        unsigned char *s = touch(pf, path, &len);
        if (!s) return;                                  /* open sea: no ground file */
        t = malloc(sizeof *t);
        int bad = adt_read(s, len, 0, t, &why);   /* the blend-map form doesn't matter for names */
        free(s);
        if (bad) { free(t); return; }                    /* the load itself will say what is wrong */
    }
    /* each ground picture */
    for (int k = 0; k < t->ntextures; ++k) free(touch(pf, t->textures[k], &len));
    /* each doodad: its model, skin, and the pictures it names */
    for (int k = 0; k < t->nmodels; ++k) {
        char base[260];
        snprintf(base, sizeof base, "%s", t->models[k]);
        char *dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        size_t ml, sl;
        snprintf(path, sizeof path, "%s.m2", base);
        unsigned char *m2 = touch(pf, path, &ml);
        snprintf(path, sizeof path, "%s00.skin", base);
        unsigned char *skin = touch(pf, path, &sl);
        M2Model mod;
        if (m2 && skin && !m2_read(m2, ml, skin, sl, &mod, &why)) {
            for (uint32_t x = 0; x < mod.ntextures; ++x) if (mod.textures[x].kind == 0 && mod.textures[x].file[0]) free(touch(pf, mod.textures[x].file, &len));
            m2_free(&mod);
        }
        free(m2); free(skin);
    }
    /* each building: its root, its parts, and its materials' pictures */
    for (int k = 0; k < t->nbuildings; ++k) {
        size_t rl;
        unsigned char *r = touch(pf, t->buildings[k], &rl);
        WmoRoot root;
        if (r && !wmo_read_root(r, rl, &root, &why)) {
            char base[260];
            snprintf(base, sizeof base, "%s", t->buildings[k]);
            char *dot = strrchr(base, '.');
            if (dot) *dot = '\0';
            for (int g = 0; g < root.ngroups; ++g) { snprintf(path, sizeof path, "%s_%03d.wmo", base, g); free(touch(pf, path, &len)); }
            for (int m = 0; m < root.nmaterials; ++m) if (root.materials[m].picture[0]) free(touch(pf, root.materials[m].picture, &len));
            wmo_free_root(&root);
        }
        free(r);
    }
    if (!*ground) { adt_free(t); free(t); }              /* the preparation's file stays with it */
}
/* }}} */

/* {{{ static work: the thread's loop */
static void *work(void *arg)
{
    Prefetch *pf = arg;
    pthread_mutex_lock(&pf->lock);
    for (;;) {
        while (pf->head == pf->tail && !pf->stopping) pthread_cond_wait(&pf->wake, &pf->lock);
        if (pf->stopping) break;
        int i = pf->queue[pf->head];
        pf->head = (pf->head + 1) % (64 * 64);
        pf->busy = 1;
        pf->working_on = i;
        pthread_mutex_unlock(&pf->lock);
        GroundPrep *ground;
        int state;
        read_tile(pf, i, &ground, &state);
        pthread_mutex_lock(&pf->lock);
        pf->busy = 0;
        pf->working_on = -1;
        ++pf->tiles_done;
        /* kept for the main thread, unless it loaded the tile itself or
         * forgot it meanwhile (then it would never be taken) */
        if (ground && (pf->skip[i] || !pf->asked[i])) { ground_prep_free(ground, 0); ground = NULL; state = 0; }
        /* already prepared: a tile forgotten while still queued, then asked
         * again, is in the queue twice; the first preparation stands */
        if (ground && pf->ready[i]) { ground_prep_free(ground, 0); ground = NULL; }
        if (ground) { ++pf->grounds_made; pf->ready[i] = ground; }
        pf->ground_state[i] = (unsigned char)state;
        pthread_cond_broadcast(&pf->idle);                  /* a take waiting on this tile */
    }
    pthread_mutex_unlock(&pf->lock);
    return NULL;
}
/* }}} */

/* {{{ prefetch_start, prefetch_prepare_ground, prefetch_tile, prefetch_wait, prefetch_counts, prefetch_stop */
Prefetch *prefetch_start(MpqChain *chain, const char *map_name, int read_ahead)
{
    Prefetch *pf = calloc(1, sizeof *pf);
    pf->chain = chain;
    snprintf(pf->map, sizeof pf->map, "%s", map_name);
    pf->working_on = -1;
    pthread_mutex_init(&pf->lock, NULL);
    pthread_cond_init(&pf->wake, NULL);
    pthread_cond_init(&pf->idle, NULL);
    /* off when asked (--no-read-ahead, to measure what it saves); when the
     * thread can't start it is off too, and says so (tiles still load, on
     * the main thread, as they would with it off) */
    if (!read_ahead) return pf;
    if (pthread_create(&pf->thread, NULL, work, pf) == 0) pf->started = 1;
    else fprintf(stderr, "prefetch: WARNING: the helper thread couldn't start; tiles load without reading ahead\n");
    return pf;
}
void prefetch_prepare_ground(Prefetch *pf, const GroundPlace *place, int (*on_card)(const char *name))
{
    pthread_mutex_lock(&pf->lock);
    pf->place = place;
    pf->on_card = on_card;
    pthread_mutex_unlock(&pf->lock);
}
void prefetch_tile(Prefetch *pf, int tx, int ty)
{
    if (!pf->started || tx < 0 || tx > 63 || ty < 0 || ty > 63) return;
    int i = tx * 64 + ty;
    pthread_mutex_lock(&pf->lock);
    if (!pf->asked[i]) {
        pf->asked[i] = 1;
        pf->skip[i] = 0;
        pf->queue[pf->tail] = i;
        pf->tail = (pf->tail + 1) % (64 * 64);
        pthread_cond_signal(&pf->wake);
    }
    pthread_mutex_unlock(&pf->lock);
}
void prefetch_wait(Prefetch *pf)
{
    if (!pf->started) return;
    pthread_mutex_lock(&pf->lock);
    while (pf->head != pf->tail || pf->busy) pthread_cond_wait(&pf->idle, &pf->lock);
    pthread_mutex_unlock(&pf->lock);
}
void prefetch_counts(Prefetch *pf, int *tiles, int *files, int *grounds_made, int *grounds_taken)
{
    pthread_mutex_lock(&pf->lock);
    *tiles = pf->tiles_done;
    *grounds_made = pf->grounds_made;
    *grounds_taken = pf->grounds_taken;
    pthread_mutex_unlock(&pf->lock);
    *files = __atomic_load_n(&pf->files_read, __ATOMIC_RELAXED);
}
void prefetch_stop(Prefetch *pf)
{
    if (!pf->started) return;
    pthread_mutex_lock(&pf->lock);
    pf->stopping = 1;
    pthread_cond_broadcast(&pf->wake);
    pthread_mutex_unlock(&pf->lock);
    pthread_join(pf->thread, NULL);
    pf->started = 0;
    for (int i = 0; i < 64 * 64; ++i) if (pf->ready[i]) { ground_prep_free(pf->ready[i], 0); pf->ready[i] = NULL; }
}
void prefetch_free(Prefetch *pf)
{
    prefetch_stop(pf);
    pthread_mutex_destroy(&pf->lock);
    pthread_cond_destroy(&pf->wake);
    pthread_cond_destroy(&pf->idle);
    free(pf);
}
/* }}} */

/* {{{ prefetch_take_ground */
int prefetch_take_ground(Prefetch *pf, int tx, int ty, GroundPrep **out)
{
    *out = NULL;
    if (!pf->started || tx < 0 || tx > 63 || ty < 0 || ty > 63) return 2;
    int i = tx * 64 + ty;
    pthread_mutex_lock(&pf->lock);
    /* the thread is preparing this very tile: waiting costs less than
     * preparing it a second time here */
    while (pf->working_on == i) pthread_cond_wait(&pf->idle, &pf->lock);
    int got = 2;                                         /* not prepared: the caller prepares it */
    if (pf->ground_state[i] == 1) { *out = pf->ready[i]; pf->ready[i] = NULL; got = 0; ++pf->grounds_taken; }
    else if (pf->ground_state[i] == 2) got = 1;             /* no ground file there */
    pf->ground_state[i] = 0;
    pf->skip[i] = 1;                                        /* loaded now: a preparation still queued is wasted */
    pthread_mutex_unlock(&pf->lock);
    return got;
}
/* }}} */

/* {{{ prefetch_keep_near: tiles more than `reach` from (tx, ty) are
 * forgotten: their preparations freed (she has turned away: about 10 MB
 * each), and they may be asked for again when she comes back */
void prefetch_keep_near(Prefetch *pf, int tx, int ty, int reach)
{
    if (!pf->started) return;
    pthread_mutex_lock(&pf->lock);
    for (int x = 0; x < 64; ++x)
        for (int y = 0; y < 64; ++y) {
            int i = x * 64 + y;
            if (abs(x - tx) <= reach && abs(y - ty) <= reach) continue;
            if (pf->ready[i]) { ground_prep_free(pf->ready[i], 0); pf->ready[i] = NULL; }
            pf->ground_state[i] = 0;
            pf->asked[i] = 0;
        }
    pthread_mutex_unlock(&pf->lock);
}
/* }}} */
