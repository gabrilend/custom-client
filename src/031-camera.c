/*
 * 031-camera.c (issue 173m) - the camera that follows her; see 031-camera.h
 *
 * For a general audience: the mouse drags and the following styles. The
 * styles are a table: each row names the setting's stored value and a
 * small rule saying where the camera should drift to this frame, if
 * anywhere. Adding a style is adding a row.
 */
#include "031-camera.h"
#include "014-settings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265358979f
#define PI_D 3.14159265358979323846

/* {{{ camera_turn_between */
float camera_turn_between(float from, float to)
{
    float d = fmodf(to - from, 2 * PI_F);
    if (d >  PI_F) d -= 2 * PI_F;
    if (d < -PI_F) d += 2 * PI_F;
    return d;
}
/* }}} */

/* {{{ static keep_pitch: within the range for the camera's distance;
 * first person may look as far up as down, third person can't go much
 * below her shoulders */
static void keep_pitch(FollowCam *c)
{
    float low = c->dist == 0 ? -CAMERA_PITCH_MAX : CAMERA_PITCH_MIN;
    if (c->pitch < low) c->pitch = low;
    if (c->pitch > CAMERA_PITCH_MAX) c->pitch = CAMERA_PITCH_MAX;
}
/* }}} */

/* {{{ camera_drag */
void camera_drag(FollowCam *c, const FollowCamInput *in, int was_right, double *facing)
{
    /* right pressed this frame: she turns to the camera's heading
     * (yaw is unwrapped; her facing is kept in 0 to 2 pi as the mover keeps it) */
    if (in->right && !was_right) *facing = fmod(fmod((double)c->yaw, 2 * PI_D) + 2 * PI_D, 2 * PI_D);
    if (!in->left && !in->right) return;

    float rate   = CAMERA_DRAG_RATE * (float)settings_num(c->settings, "cameraYawMoveSpeed") / 180.0f;
    float invert = settings_num(c->settings, "mouseInvertPitch") != 0 ? -1.0f : 1.0f;
    c->yaw   -= in->dx * rate;
    c->pitch += in->dy * rate * invert;            /* dragging down raises the camera, as in the original */
    keep_pitch(c);
    if (in->right) *facing = fmod(fmod((double)c->yaw, 2 * PI_D) + 2 * PI_D, 2 * PI_D);
}
/* }}} */

/* {{{ the following styles
 * Each rule answers, for this frame: should the heading drift, and should
 * the pitch drift? The heading drifts toward behind her (her facing); the
 * pitch toward the camera's resting pitch. Buttons held are dealt with
 * before any rule is asked, so the rules don't see them.
 *
 * The stored values are the original's (cameraSmoothStyle; its menu lists
 * them in the order 1, 4, 2, 0). Style 2's pitch drift is this client's
 * reading of the original's "always prefer being behind your character";
 * the original's tooltip doesn't say whether the pitch returns too. */
#define DRIFT_YAW   1
#define DRIFT_PITCH 2

static int style_horizontal_when_moving(int moving) { return moving ? DRIFT_YAW : 0; }
static int style_when_moving(int moving)            { return moving ? DRIFT_YAW | DRIFT_PITCH : 0; }
static int style_always(int moving)                 { (void)moving; return DRIFT_YAW | DRIFT_PITCH; }
static int style_never(int moving)                  { (void)moving; return 0; }

static const struct {
    const char *value;                 /* cameraSmoothStyle's stored value */
    int (*drift)(int moving);
} STYLES[] = {
    { "1", style_horizontal_when_moving },   /* Only horizontal when moving (the default) */
    { "4", style_when_moving            },   /* Only when moving */
    { "2", style_always                 },   /* Always adjust camera */
    { "0", style_never                  },   /* Never adjust camera */
};
#define STYLE_COUNT ((int)(sizeof STYLES / sizeof STYLES[0]))
/* }}} */

/* {{{ static step_toward: move `at` toward `to` by at most `most`, the shorter way */
static float step_toward(float at, float to, float most)
{
    float d = camera_turn_between(at, to);
    if (fabsf(d) <= most) return at + d;
    return at + (d > 0 ? most : -most);
}
/* }}} */

/* {{{ camera_tilt */
void camera_tilt(FollowCam *c, const FollowCamInput *in, int moving, float slope)
{
    if (settings_num(c->settings, "cameraTerrainTilt") == 0 || !moving || in->left || in->right) return;
    float target = c->rest_pitch - atanf(slope);
    float most = (float)settings_num(c->settings, "cameraYawSmoothSpeed") * PI_F / 180.0f * in->dt;
    c->pitch = step_toward(c->pitch, target, most);
    keep_pitch(c);
}
/* }}} */

/* {{{ camera_zoom */
void camera_zoom(FollowCam *c, float wheel, float zoom_max)
{
    if (wheel > 0 && c->dist > 0) {                /* in: nearer, or into first person from the nearest */
        c->dist = c->dist <= CAMERA_ZOOM_MIN + 1e-3f ? 0 : c->dist * 0.85f;
        if (c->dist > 0 && c->dist < CAMERA_ZOOM_MIN) c->dist = CAMERA_ZOOM_MIN;
    } else if (wheel < 0) {                        /* out: from first person to the nearest, else farther */
        c->dist = c->dist == 0 ? CAMERA_ZOOM_MIN : c->dist * 1.18f;
    }
    if (c->dist > zoom_max) c->dist = zoom_max;    /* first person stays first person; any other distance is held in range */
    if (c->dist > 0 && c->dist < CAMERA_ZOOM_MIN) c->dist = CAMERA_ZOOM_MIN;
    keep_pitch(c);
}
/* }}} */

/* {{{ camera_bob
 * The lift is |sin| of the phase over one step: 0 at each foot-down, the
 * full height mid-step. Standing, the phase runs on to the next whole step
 * (lift 0) and stops there. */
float camera_bob(const Settings *st, float *phase, float moved, float dt)
{
    if (moved > 0) *phase += moved;
    else {
        float next = ceilf(*phase / CAMERA_BOB_STEP - 1e-4f) * CAMERA_BOB_STEP;
        *phase = fminf(*phase + 2.0f * dt, next);
    }
    *phase = fmodf(*phase, CAMERA_BOB_STEP * 1000);   /* kept small, so the float holds its precision */
    if (settings_num(st, "cameraBobbing") == 0) return 0;
    return CAMERA_BOB_HEIGHT * fabsf(sinf(PI_F * *phase / CAMERA_BOB_STEP));
}
/* }}} */

/* {{{ camera_follow */
void camera_follow(FollowCam *c, const FollowCamInput *in, double turned, int moving, double facing)
{
    if (!in->left) c->yaw += (float)turned;        /* the keys turn the camera with her */
    if (in->left || in->right) return;            /* a drag beats every style */

    /* the style's row; the store only accepts the table's values, so a
     * miss means the store's choices and this table disagree */
    const char *chosen = settings_text(c->settings, "cameraSmoothStyle");
    int row = 0;
    while (row < STYLE_COUNT && strcmp(STYLES[row].value, chosen)) ++row;
    if (row == STYLE_COUNT) {
        fprintf(stderr, "031-camera.c: cameraSmoothStyle \"%s\" has no row in the styles table\n", chosen);
        exit(1);
    }

    int   drift = STYLES[row].drift(moving);
    float most  = (float)settings_num(c->settings, "cameraYawSmoothSpeed") * PI_F / 180.0f * in->dt;   /* degrees a second */
    if (drift & DRIFT_YAW)   c->yaw   = step_toward(c->yaw, (float)facing, most);
    if (drift & DRIFT_PITCH) c->pitch = step_toward(c->pitch, c->rest_pitch, most);
}
/* }}} */
