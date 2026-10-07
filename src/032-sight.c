/*
 * 032-sight.c (issue 173u) - what the camera can see; see 032-sight.h
 *
 * For a general audience: the six walls come straight out of the
 * camera's combined matrix (each wall is a sum or difference of its
 * rows: the standard way, Gribb and Hartmann's); a ball is outside when
 * it lies wholly behind any one wall.
 */
#include "032-sight.h"

#define RAYMATH_STATIC_INLINE              /* no raylib library in the tests: the maths compiled in here */
#include "raymath.h"

#include <math.h>

/* {{{ sight_from
 * Raylib's Matrix fields m0..m15 are column by column; row r of the
 * matrix is (m[r], m[4 + r], m[8 + r], m[12 + r]). Clip-space x, y, z
 * within -w..w: left = row3 + row0, right = row3 - row0, bottom = row3 +
 * row1, top = row3 - row1, near = row3 + row2, far = row3 - row2. */
void sight_from(Sight *s, Matrix vp)
{
    const float *m = MatrixToFloatV(vp).v;
    float row[4][4];
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) row[r][c] = m[c * 4 + r];
    static const int AXIS[6] = { 0, 0, 1, 1, 2, 2 };
    static const float SIGN[6] = { 1, -1, 1, -1, 1, -1 };
    for (int p = 0; p < 6; ++p) {
        float *pl = s->plane[p];
        for (int c = 0; c < 4; ++c) pl[c] = row[3][c] + SIGN[p] * row[AXIS[p]][c];
        float len = sqrtf(pl[0] * pl[0] + pl[1] * pl[1] + pl[2] * pl[2]);
        if (len > 0) for (int c = 0; c < 4; ++c) pl[c] /= len;
    }
}
/* }}} */

/* {{{ sight_ball */
int sight_ball(const Sight *s, Vector3 c, float radius)
{
    for (int p = 0; p < 6; ++p) {
        const float *pl = s->plane[p];
        if (pl[0] * c.x + pl[1] * c.y + pl[2] * c.z + pl[3] < -radius) return 0;
    }
    return 1;
}
/* }}} */
