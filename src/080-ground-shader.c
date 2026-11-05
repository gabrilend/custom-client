/*
 * 080-ground-shader.c (issues 173f, 302, 1109) - the ground's shader, as
 * text; see 080-ground-shader.h
 *
 * For a general audience: the program the graphics card runs for each
 * point of the ground.
 */
#include "080-ground-shader.h"

const char *const GROUND_VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec2 vertexTexCoord; in vec2 vertexTexCoord2; in vec4 vertexColor; in vec4 vertexTangent;\n"
    "uniform mat4 mvp; uniform vec3 eye;\n"
    "out vec3 n; out vec2 uv; out vec2 uv2; out vec3 tint; out float dist; out vec3 pos; flat out vec4 lay;\n"
    "void main() {\n"
    "  pos = vertexPosition; lay = vertexTangent;\n"
    "  n = vertexNormal; uv = vertexTexCoord; uv2 = vertexTexCoord2; tint = vertexColor.rgb * (255.0 / 127.0);\n"
    "  dist = length(vertexPosition - eye);\n"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

const char *const GROUND_FS =
    "#version 330\n"
    "in vec3 n; in vec2 uv; in vec2 uv2; in vec3 tint; in float dist; in vec3 pos; flat in vec4 lay; out vec4 frag;\n"
    "uniform sampler2D blend; uniform sampler2DArray layers; uniform sampler2D cells; uniform int has_cells;\n"
    "uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "uniform vec4 blobs[8]; uniform int nblobs;\n"
    /* a WC3 piece: subtile `sub` of layer `layer`, at `f` in the cell */
    "vec4 piece(float layer, float sub, vec2 f, vec2 gx, vec2 gy) {\n"
    "  vec2 at = (vec2(mod(sub, 4.0), floor(sub / 4.0)) + clamp(f, 1.0 / 128.0, 127.0 / 128.0)) * 0.25;\n"
    "  return textureGrad(layers, vec3(at, layer), gx, gy);\n"
    "}\n"
    "void main() {\n"
    "  vec4 a = texture(blend, uv2);\n"
    "  vec2 gx = dFdx(uv) * 0.25, gy = dFdy(uv) * 0.25;\n"
    "  ivec2 cell = clamp(ivec2(floor(uv)), ivec2(0), ivec2(7));\n"
    "  vec4 lays = vec4(0.0), subs = vec4(255.0);\n"
    "  if (has_cells != 0) {\n"
    "    ivec2 at = ivec2(floor(uv2 * 16.0)) * 8 + cell;\n"
    "    lays = floor(texelFetch(cells, ivec2(at.x * 2, at.y), 0) * 255.0 + 0.5);\n"
    "    subs = floor(texelFetch(cells, ivec2(at.x * 2 + 1, at.y), 0) * 255.0 + 0.5);\n"
    "  }\n"
    "  vec3 c;\n"
    "  if (subs.x < 255.0) {\n"
    "    vec2 f = uv - vec2(cell);\n"
    "    c = piece(lays.x, subs.x, f, gx, gy).rgb;\n"
    "    for (int q = 1; q < 4; ++q)\n"
    "      if (subs[q] < 255.0) { vec4 s = piece(lays[q], subs[q], f, gx, gy); c = mix(c, s.rgb, s.a); }\n"
    "  } else {\n"
    "    c = texture(layers, vec3(uv, lay.x)).rgb;\n"
    "    c = mix(c, texture(layers, vec3(uv, lay.y)).rgb, a.r);\n"
    "    c = mix(c, texture(layers, vec3(uv, lay.z)).rgb, a.g);\n"
    "    c = mix(c, texture(layers, vec3(uv, lay.w)).rgb, a.b);\n"
    "  }\n"
    "  c *= tint;\n"
    "  c *= mix(0.6, 1.0, a.a);\n"
    "  for (int i = 0; i < nblobs; ++i) {\n"
    "    float d = length(pos.xy - blobs[i].xy) / blobs[i].w;\n"
    "    float above = clamp(1.0 - abs(pos.z - blobs[i].z) / 3.0, 0.0, 1.0);\n"
    "    c *= 1.0 - 0.6 * (1.0 - smoothstep(0.3, 1.0, d)) * above;\n"
    "  }\n"
    "  vec3 light = shade_col + sun_col * max(dot(normalize(n), sun_dir), 0.0);\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  frag = vec4(mix(c * light, fog_col, fog), 1.0);\n"
    "}\n";
