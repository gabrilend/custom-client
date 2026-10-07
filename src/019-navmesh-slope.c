/*
 * 019-navmesh-slope.c (issue 173k) - see 019-navmesh-slope.h.
 */
#include "019-navmesh-slope.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MMAP_MAGIC   0x4d4d4150u
#define MMAP_VERSION 19u

/* {{{ navmesh_slope_read */
float navmesh_slope_read(const char *dir, int map_id, char *why, int why_len)
{
    char prefix[8], name[256] = { 0 };
    snprintf(prefix, sizeof prefix, "%03d", map_id);
    DIR *d = opendir(dir);
    if (!d) { snprintf(why, why_len, "can't open %s", dir); return -1; }
    /* the first tile file of the map, by name order (as the Lua's `ls` gives) */
    for (struct dirent *e; (e = readdir(d)); ) {
        size_t n = strlen(e->d_name);
        if (strncmp(e->d_name, prefix, 3) == 0 && n > 7 && strcmp(e->d_name + n - 7, ".mmtile") == 0)
            if (!name[0] || strcmp(e->d_name, name) < 0) snprintf(name, sizeof name, "%s", e->d_name);
    }
    closedir(d);
    if (!name[0]) { snprintf(why, why_len, "no navigation tile for map %d in %s (scripts/extract-maps makes them)", map_id, dir); return -1; }

    char path[800];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    unsigned char h[24];
    if (!f || fread(h, 1, 24, f) != 24) { if (f) fclose(f); snprintf(why, why_len, "%s: can't read its header", path); return -1; }
    fclose(f);
    uint32_t magic = (uint32_t)h[0] | ((uint32_t)h[1] << 8) | ((uint32_t)h[2] << 16) | ((uint32_t)h[3] << 24);
    uint32_t ver   = (uint32_t)h[8] | ((uint32_t)h[9] << 8) | ((uint32_t)h[10] << 16) | ((uint32_t)h[11] << 24);
    if (magic != MMAP_MAGIC) { snprintf(why, why_len, "%s is not a navigation tile (no MMAP mark)", path); return -1; }
    if (ver != MMAP_VERSION) {
        snprintf(why, why_len, "%s is navigation tile version %u; this reader knows version %u's header", path, ver, MMAP_VERSION);
        return -1;
    }
    float angle;
    memcpy(&angle, h + 20, 4);
    return angle;
}
/* }}} */
