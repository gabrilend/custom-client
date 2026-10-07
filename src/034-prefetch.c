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

/* {{{ state: the queue (tile numbers tx * 64 + ty), which tiles were ever
 * asked, and the thread */
static MpqChain       *chain_;
static char            map_[64];
static int             queue_[64 * 64], head_, tail_;
static unsigned char   asked_[64 * 64];
static int             busy_, stopping_, started_;
static int             tiles_done_, files_read_, grounds_made_, grounds_taken_;
/* the prepared ground (173u): per tile, 0 none, 1 prepared (in ready_),
 * 2 the map has no ground file there; skip_: the main thread loaded it
 * itself, so a queued preparation would be wasted; working_on_: the tile
 * the thread is on, -1 none (a take waits for it) */
static const GroundPlace *place_;
static int           (*on_card_)(const char *name);
static GroundPrep     *ready_[64 * 64];
static unsigned char   ground_state_[64 * 64], skip_[64 * 64];
static int             working_on_ = -1;
static pthread_t       thread_;
static pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wake_ = PTHREAD_COND_INITIALIZER, idle_ = PTHREAD_COND_INITIALIZER;
/* }}} */

/* {{{ static touch: a file read through the chain (and so kept), then let go */
static unsigned char *touch(const char *name, size_t *len)
{
    const char *from;
    unsigned char *s = mpq_chain_read(chain_, name, len, &from);
    if (s) __atomic_add_fetch(&files_read_, 1, __ATOMIC_RELAXED);
    return s;
}
/* }}} */

/* {{{ static read_tile: everything tile i's load will read, and its
 * ground prepared when asked for; `ground` gets the preparation (or
 * stays NULL) and `state` its kind (as ground_state_) */
static void read_tile(int i, GroundPrep **ground, int *state)
{
    int tx = i / 64, ty = i % 64;
    char path[300];
    size_t len;
    WWhy why;
    AdtTile *t = NULL;
    *ground = NULL;
    *state = 0;
    pthread_mutex_lock(&lock_);
    int prepare = place_ && !skip_[i];
    pthread_mutex_unlock(&lock_);
    if (prepare) {
        /* the ground prepared: its file read (and so kept) on the way */
        int got = ground_prep_tile(place_, tx, ty, on_card_, ground, &why);
        if (got == W_ERR_NOT_FOUND) { *state = 2; return; }   /* open sea: no ground file */
        if (got == W_OK) { *state = 1; t = (*ground)->adt; __atomic_add_fetch(&files_read_, 1, __ATOMIC_RELAXED); }
        /* any other code: the main thread prepares it again when it loads it, and says what is wrong */
    }
    if (!t) {
        snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", map_, map_, ty, tx);
        unsigned char *s = touch(path, &len);
        if (!s) return;                                  /* open sea: no ground file */
        t = malloc(sizeof *t);
        int bad = adt_read(s, len, 0, t, &why);   /* the blend-map form doesn't matter for names */
        free(s);
        if (bad) { free(t); return; }                    /* the load itself will say what is wrong */
    }
    /* each ground picture */
    for (int k = 0; k < t->ntextures; ++k) free(touch(t->textures[k], &len));
    /* each doodad: its model, skin, and the pictures it names */
    for (int k = 0; k < t->nmodels; ++k) {
        char base[260];
        snprintf(base, sizeof base, "%s", t->models[k]);
        char *dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        size_t ml, sl;
        snprintf(path, sizeof path, "%s.m2", base);
        unsigned char *m2 = touch(path, &ml);
        snprintf(path, sizeof path, "%s00.skin", base);
        unsigned char *skin = touch(path, &sl);
        M2Model mod;
        if (m2 && skin && !m2_read(m2, ml, skin, sl, &mod, &why)) {
            for (uint32_t x = 0; x < mod.ntextures; ++x) if (mod.textures[x].kind == 0 && mod.textures[x].file[0]) free(touch(mod.textures[x].file, &len));
            m2_free(&mod);
        }
        free(m2); free(skin);
    }
    /* each building: its root, its parts, and its materials' pictures */
    for (int k = 0; k < t->nbuildings; ++k) {
        size_t rl;
        unsigned char *r = touch(t->buildings[k], &rl);
        WmoRoot root;
        if (r && !wmo_read_root(r, rl, &root, &why)) {
            char base[260];
            snprintf(base, sizeof base, "%s", t->buildings[k]);
            char *dot = strrchr(base, '.');
            if (dot) *dot = '\0';
            for (int g = 0; g < root.ngroups; ++g) { snprintf(path, sizeof path, "%s_%03d.wmo", base, g); free(touch(path, &len)); }
            for (int m = 0; m < root.nmaterials; ++m) if (root.materials[m].picture[0]) free(touch(root.materials[m].picture, &len));
            wmo_free_root(&root);
        }
        free(r);
    }
    if (!*ground) { adt_free(t); free(t); }              /* the preparation's file stays with it */
}
/* }}} */

/* {{{ static work: the thread's loop */
static void *work(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&lock_);
    for (;;) {
        while (head_ == tail_ && !stopping_) pthread_cond_wait(&wake_, &lock_);
        if (stopping_) break;
        int i = queue_[head_];
        head_ = (head_ + 1) % (64 * 64);
        busy_ = 1;
        working_on_ = i;
        pthread_mutex_unlock(&lock_);
        GroundPrep *ground;
        int state;
        read_tile(i, &ground, &state);
        pthread_mutex_lock(&lock_);
        busy_ = 0;
        working_on_ = -1;
        ++tiles_done_;
        /* kept for the main thread, unless it loaded the tile itself or
         * forgot it meanwhile (then it would never be taken) */
        if (ground && (skip_[i] || !asked_[i])) { ground_prep_free(ground, 0); ground = NULL; state = 0; }
        /* already prepared: a tile forgotten while still queued, then asked
         * again, is in the queue twice; the first preparation stands */
        if (ground && ready_[i]) { ground_prep_free(ground, 0); ground = NULL; }
        if (ground) { ++grounds_made_; ready_[i] = ground; }
        ground_state_[i] = (unsigned char)state;
        pthread_cond_broadcast(&idle_);                  /* a take waiting on this tile */
    }
    pthread_mutex_unlock(&lock_);
    return NULL;
}
/* }}} */

/* {{{ prefetch_start, prefetch_prepare_ground, prefetch_tile, prefetch_wait, prefetch_counts, prefetch_stop */
void prefetch_start(MpqChain *chain, const char *map_name)
{
    chain_ = chain;
    snprintf(map_, sizeof map_, "%s", map_name);
    stopping_ = 0;
    if (pthread_create(&thread_, NULL, work, NULL) == 0) started_ = 1;
    else fprintf(stderr, "prefetch: the helper thread couldn't start; tiles load without reading ahead\n");
}
void prefetch_prepare_ground(const GroundPlace *place, int (*on_card)(const char *name))
{
    pthread_mutex_lock(&lock_);
    place_ = place;
    on_card_ = on_card;
    pthread_mutex_unlock(&lock_);
}
void prefetch_tile(int tx, int ty)
{
    if (!started_ || tx < 0 || tx > 63 || ty < 0 || ty > 63) return;
    int i = tx * 64 + ty;
    pthread_mutex_lock(&lock_);
    if (!asked_[i]) {
        asked_[i] = 1;
        skip_[i] = 0;
        queue_[tail_] = i;
        tail_ = (tail_ + 1) % (64 * 64);
        pthread_cond_signal(&wake_);
    }
    pthread_mutex_unlock(&lock_);
}
void prefetch_wait(void)
{
    if (!started_) return;
    pthread_mutex_lock(&lock_);
    while (head_ != tail_ || busy_) pthread_cond_wait(&idle_, &lock_);
    pthread_mutex_unlock(&lock_);
}
void prefetch_counts(int *tiles, int *files, int *grounds_made, int *grounds_taken)
{
    pthread_mutex_lock(&lock_);
    *tiles = tiles_done_;
    *grounds_made = grounds_made_;
    *grounds_taken = grounds_taken_;
    pthread_mutex_unlock(&lock_);
    *files = __atomic_load_n(&files_read_, __ATOMIC_RELAXED);
}
void prefetch_stop(void)
{
    if (!started_) return;
    pthread_mutex_lock(&lock_);
    stopping_ = 1;
    pthread_cond_broadcast(&wake_);
    pthread_mutex_unlock(&lock_);
    pthread_join(thread_, NULL);
    started_ = 0;
    for (int i = 0; i < 64 * 64; ++i) if (ready_[i]) { ground_prep_free(ready_[i], 0); ready_[i] = NULL; }
}
/* }}} */

/* {{{ prefetch_take_ground */
int prefetch_take_ground(int tx, int ty, GroundPrep **out)
{
    *out = NULL;
    if (!started_ || tx < 0 || tx > 63 || ty < 0 || ty > 63) return 2;
    int i = tx * 64 + ty;
    pthread_mutex_lock(&lock_);
    /* the thread is preparing this very tile: waiting costs less than
     * preparing it a second time here */
    while (working_on_ == i) pthread_cond_wait(&idle_, &lock_);
    int got = 2;                                         /* not prepared: the caller prepares it */
    if (ground_state_[i] == 1) { *out = ready_[i]; ready_[i] = NULL; got = 0; ++grounds_taken_; }
    else if (ground_state_[i] == 2) got = 1;             /* no ground file there */
    ground_state_[i] = 0;
    skip_[i] = 1;                                        /* loaded now: a preparation still queued is wasted */
    pthread_mutex_unlock(&lock_);
    return got;
}
/* }}} */

/* {{{ prefetch_keep_near: tiles more than `reach` from (tx, ty) are
 * forgotten: their preparations freed (she has turned away: about 10 MB
 * each), and they may be asked for again when she comes back */
void prefetch_keep_near(int tx, int ty, int reach)
{
    if (!started_) return;
    pthread_mutex_lock(&lock_);
    for (int x = 0; x < 64; ++x)
        for (int y = 0; y < 64; ++y) {
            int i = x * 64 + y;
            if (abs(x - tx) <= reach && abs(y - ty) <= reach) continue;
            if (ready_[i]) { ground_prep_free(ready_[i], 0); ready_[i] = NULL; }
            ground_state_[i] = 0;
            asked_[i] = 0;
        }
    pthread_mutex_unlock(&lock_);
}
/* }}} */
