# 202 — Math Primitives

## Current behavior

There is no project-owned math library. raylib ships `raymath.h` (a
single-header set of vector/matrix helpers), but it ties our internal
types to raylib's `Vector3` / `Matrix` structs, and that's a coupling we
do not want at the boundaries between subsystems.

## Intended behavior

A small project-owned math library with our own types:

```c
typedef struct { float x, y; }       vec2_t;
typedef struct { float x, y, z; }    vec3_t;
typedef struct { float x, y, z, w; } vec4_t;
typedef struct { float m[16]; }      mat4_t;    /* column-major, GL order */
typedef struct { float x, y, z, w; } quat_t;    /* xyzw, w is real        */
```

Operations are free functions, not methods:

```c
vec3_t vec3_add (vec3_t a, vec3_t b);
vec3_t vec3_sub (vec3_t a, vec3_t b);
vec3_t vec3_scl (vec3_t a, float s);
float  vec3_dot (vec3_t a, vec3_t b);
vec3_t vec3_cross(vec3_t a, vec3_t b);
float  vec3_len (vec3_t a);
vec3_t vec3_norm(vec3_t a);

mat4_t mat4_identity(void);
mat4_t mat4_mul(mat4_t a, mat4_t b);
mat4_t mat4_translate(vec3_t t);
mat4_t mat4_rotate_axis_angle(vec3_t axis, float radians);
mat4_t mat4_scale(vec3_t s);
mat4_t mat4_perspective(float fov_y_rad, float aspect, float n, float f);
mat4_t mat4_ortho     (float l, float r, float b, float t, float n, float f);
mat4_t mat4_look_at   (vec3_t eye, vec3_t target, vec3_t up);

quat_t quat_from_axis_angle(vec3_t axis, float radians);
quat_t quat_mul(quat_t a, quat_t b);
quat_t quat_slerp(quat_t a, quat_t b, float t);
mat4_t mat4_from_quat(quat_t q);
```

Two helpers that just bridge into raylib when crossing the boundary:

```c
Vector3 vec3_to_raylib(vec3_t);
vec3_t  vec3_from_raylib(Vector3);
Matrix  mat4_to_raylib(mat4_t);
mat4_t  mat4_from_raylib(Matrix);
```

Calling these is the only place internal types touch raylib types.
Subsystems never expose `Vector3` in their public headers.

## Suggested implementation steps

1. Create `src/math/` subsystem. Header pair (single header is tempting
   but the inline definitions get heavy; split keeps debuggers happy).
2. Implement vec2/3/4 ops.
3. Implement mat4 ops. Match raymath's conventions (column-major,
   right-handed, +Y up… **wait**: we use +Z up per
   [docs/005](../docs/005-rendering-pipeline.md). Make sure the
   `mat4_look_at` + `mat4_perspective` pair agrees with that, and write
   a test that confirms it.
3. Implement quat ops + `mat4_from_quat`. Worth eyeballing the slerp
   short-vs-long-arc handling — easy to get wrong on first try.
4. Implement the four raylib bridge helpers. They're trivial element
   copies; the value is having one obvious place to look when a
   coordinate-system bug appears.
5. Tests: ~30 short asserts covering the common identities (rotate by
   2π returns to identity, look-at + perspective produces an expected
   matrix for known eye/target, quat slerp midpoint matches a
   half-angle rotation, the raylib bridge round-trips bytewise).

## Acceptance

- All tests pass.
- The output of `mat4_perspective` × `mat4_look_at` × a known world
  point matches a hand-computed clip-space coordinate to within 1e-5.
- Zero warnings under `-Wfloat-equal -Wdouble-promotion`.

## Stress markers

The +Z-up vs. +Y-up split between our world and raylib's defaults
(`Camera3D` assumes +Y up by default) is the kind of disagreement that
won't surface until [203](./203-shader-pipeline.md) or
[207](./207-camera.md) and then will be deeply confusing. The bridge
helpers do not silently swap axes — they just copy components. Any axis
swap happens explicitly in the camera setup, never hidden inside the
math layer.

## Related

- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: [101](./101-build-system.md).
- Blocks: [203](./203-shader-pipeline.md), [205](./205-sprite-renderer.md),
  [207](./207-camera.md), and all later geometry/rendering issues.
