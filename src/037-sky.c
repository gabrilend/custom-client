/*
 * 037-sky.c (issue 173n) - the sky dome; see 037-sky.h
 *
 * For a general audience: builds the bowl once (rings of points round
 * the camera), each point knowing which of the light's five sky colours
 * it takes; each frame only the five colours go to the graphics card
 * (173u), and the bowl is drawn behind everything.
 */
#include "037-sky.h"
#include "042-doodads.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the rings */
#define SEGMENTS 48
static const float RING_DEG[] = { -30, 0, 6, 15, 35, 90 };
static const int   RING_SKY[] = { 4, 4, 3, 2, 1, 0 };           /* which sky colour each ring takes */
#define RINGS ((int)(sizeof RING_DEG / sizeof RING_DEG[0]))
#define RADIUS 900.0f                                            /* inside the far plane (1600) */
/* the dome's shader: each point's sky colour number rides in its colour's
 * red (0-4); the colours themselves are five shader values, set each
 * frame. No light, no fog: the sky is the fog's backdrop. */
static const char *SKY_VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec4 vertexColor; uniform mat4 mvp; uniform vec3 sky[5]; out vec3 c;\n"
    "void main() { c = sky[int(vertexColor.r * 255.0 + 0.5)]; gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
static const char *SKY_FS =
    "#version 330\n"
    "in vec3 c; out vec4 frag; void main() { frag = vec4(min(c, vec3(1.0)), 1.0); }\n";
/* }}} */

/* {{{ a sky (the handle: 037-sky.h): the dome, its material and shader
 * value's place, and the skybox now drawn ("" none) */
struct Sky {
    Mesh     dome;
    Material plain;
    int      loc_sky;
    char     box_path[128];
};
/* }}} */

/* {{{ sky_new, sky_free */
void sky_free(Sky *sk)
{
    UnloadMesh(sk->dome);
    UnloadShader(sk->plain.shader);
    free(sk);
}
Sky *sky_new(void)
{
    Sky *sk = calloc(1, sizeof *sk);
    Mesh m = { 0 };
    m.vertexCount = RINGS * (SEGMENTS + 1);
    m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
    m.colors = MemAlloc(4 * m.vertexCount);
    for (int r = 0; r < RINGS; ++r)
        for (int s = 0; s <= SEGMENTS; ++s) {
            float el = RING_DEG[r] * DEG2RAD, az = (float)s / SEGMENTS * 2 * PI;
            int i = r * (SEGMENTS + 1) + s;
            m.vertices[i * 3]     = cosf(el) * cosf(az) * RADIUS;
            m.vertices[i * 3 + 1] = cosf(el) * sinf(az) * RADIUS;
            m.vertices[i * 3 + 2] = sinf(el) * RADIUS;
            m.colors[i * 4] = (unsigned char)RING_SKY[r];      /* which sky colour (sky[] in the shader) */
            m.colors[i * 4 + 3] = 255;
        }
    m.triangleCount = (RINGS - 1) * SEGMENTS * 2;
    m.indices = MemAlloc(sizeof(unsigned short) * m.triangleCount * 3);
    int n = 0;
    for (int r = 0; r + 1 < RINGS; ++r)
        for (int s = 0; s < SEGMENTS; ++s) {
            unsigned short a = (unsigned short)(r * (SEGMENTS + 1) + s), b = (unsigned short)(a + 1);
            unsigned short c = (unsigned short)(a + SEGMENTS + 1), d = (unsigned short)(c + 1);
            unsigned short tri[6] = { a, b, c, b, d, c };
            memcpy(m.indices + n, tri, sizeof tri);
            n += 6;
        }
    UploadMesh(&m, false);                                   /* sent once; the colours come as shader values */
    sk->dome = m;
    sk->plain = LoadMaterialDefault();
    sk->plain.shader = LoadShaderFromMemory(SKY_VS, SKY_FS);
    sk->loc_sky = GetShaderLocation(sk->plain.shader, "sky");
    return sk;
}
/* }}} */

/* {{{ sky_light_shader */
void sky_light_shader(Shader sh, const LightNow *l)
{
    float range[2] = { l->fog_start, l->fog_end };
    SetShaderValue(sh, GetShaderLocation(sh, "sun_dir"), l->sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "sun_col"), l->sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "shade_col"), l->shade, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "fog_col"), l->fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "fog_range"), range, SHADER_UNIFORM_VEC2);
}
/* }}} */

/* {{{ sky_draw */
/* the skybox the light names, handed to the doodads when it changes */
static void skybox_for(Sky *sk, const LightNow *light)
{
    if (!strcmp(light->skybox, sk->box_path)) return;
    snprintf(sk->box_path, sizeof sk->box_path, "%s", light->skybox);
    doodads_skybox(sk->box_path);
    if (sk->box_path[0]) printf("sky: the skybox %s\n", sk->box_path);
}

void sky_draw(Sky *sk, Vector3 eye, const LightNow *light)
{
    skybox_for(sk, light);
    SetShaderValueV(sk->plain.shader, sk->loc_sky, light->sky, SHADER_UNIFORM_VEC3, 5);   /* the five colours, overhead to horizon */
    rlDisableDepthTest();
    rlDisableDepthMask();
    DrawMesh(sk->dome, sk->plain, MatrixTranslate(eye.x, eye.y, eye.z));
    rlDrawRenderBatchActive();
    doodads_draw_skybox(eye);
    rlEnableDepthMask();
    rlEnableDepthTest();
}
/* }}} */
