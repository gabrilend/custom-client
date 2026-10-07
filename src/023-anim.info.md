# 023-anim

*Written by `scripts/generate-info` from `023-anim.h`; don't edit, re-run it.*

023-anim.h (issue 173i) - a model's bones and animations

For a general audience: a character's model has a skeleton: a few dozen
bones, each hanging from a parent, each point of the body tied to up
to four bones by weights. Every animation (stand, run, jump ...) says,
bone by bone, where the bone moves, how it turns and how it stretches,
at a list of moments. This reads the skeleton and the animation list,
and for any animation at any moment works out where each bone is, so
the body can be bent to match (skinning). Some animations keep their
moments in separate .anim files beside the model; those are attached
when first needed.

Layouts (3.3.5a, model version 264; little-endian):
Header: 20 global sequence count/offset (u32 lengths, ms), 28 animation
  count/offset, 44 bone count/offset, 60 point count/offset.
Animation (64 bytes): 0 id (AnimationData.dbc: 0 Stand, 4 Walk, 5 Run,
  13 Walk backwards, 37 Jump start, 38 Jump, 39 Jump end, 40 Fall ...),
  2 variation, 4 length (ms), 8 move speed (yards a second the
  animation was made for), 12 flags (0x20: its moments are in the .m2;
  otherwise in <model><id:4>-<variation:2>.anim; 0x40: an alias of the
  animation at 62), 16 how often it is picked among variations, 28
  blend time (ms), 60 next variation, 62 alias.
Bone (88 bytes): 0 key bone id, 4 flags (0x8: always faces the camera,
  "spherical billboard"; 0x40: turns only round its upright to face it,
  "cylindrical"; 0x10 and 0x20 the same round its other axes, treated as
  0x40), 8 parent (-1 none), 16 move
  track, 36 turn track, 56 stretch track, 76 pivot (3 floats).
Track (20 bytes): 0 interpolation (0 none, 1 linear, 2-3 curves whose
  values come in threes: value, in, out), 2 global sequence (-1 none),
  4 moments (count, offset of a list of per-animation [count, offset]
  arrays of u32 ms), 12 values (the same shape). Moves and stretches
  are 3 floats; turns are 4 signed 16-bit numbers (x, y, z, w; n < 0:
  (n + 32768) / 32767, else (n - 32767) / 32767).
Point (48 bytes): 12 four bone weights (u8, sum 255), 16 four bone
  numbers (u8, into the bone list).

A bone's pose: parent's pose * move to pivot * move * turn * stretch *
move back from pivot, as the original builds it.

## What it offers

- `type AnimSequence`
- `type AnimTrack`
- `type AnimBone`
- `type Anim`
- `int anim_read(const unsigned char *m2, size_t len, Anim *out, char *why, int why_len);`
  Reads the skeleton and animation list of a model file (copied).
- `void anim_free(Anim *a);`
- `int anim_find(const Anim *a, int id, int variation);`
  The animation with this id and variation (following aliases), or -1.
- `int anim_needs_file(const Anim *a, int seq);`
  1 when animation `seq` keeps its moments in a .anim file not yet
  attached (not for aliases, which have none of their own); its name is
  <model path without .m2><id:4>-<variation:2>.anim.
- `int anim_track_value(const Anim *a, const AnimTrack *tr, int seq, uint32_t ms, uint32_t global_ms, int kind, float *out);`
  A track's value at a moment of animation `seq` (blended between the
  two recorded moments around it), read as `kind`: ANIM_VEC3 3 floats,
  ANIM_TURN a turn (4 floats, x y z w), ANIM_FLOAT 1 float, ANIM_BYTE 1
  byte. Tracks other than bones' (particle settings, 173o) use it too.
  0 when the track has nothing for this animation (`out` untouched).
- `void anim_uv(const Anim *a, int uvanim, int seq, uint32_t ms, uint32_t global_ms, float out[4]);`
  A picture-position animation's state at a moment (header 96 count, 100
  offset; 60 bytes each: slide, a vec3 track; turn, a quaternion track;
  stretch, a vec3 track): out = slide x, slide y, stretch x, stretch y
  (positions become position * stretch + slide). The turn isn't applied
  (this client: none seen yet that turns).
- `void anim_unit_tint(const Anim *a, int colour, int fade, int seq, uint32_t ms, uint32_t global_ms, float rgba[4]);`
  A unit's animated tint at a moment of animation `seq` (022-m2.h's M2Unit
  colour and fade; -1 for none): red, green, blue and opacity, 0-1,
  starting from white and opaque. The colour track pair is at the
  header's 76 (40 bytes each: a colour track, then an opacity share),
  the transparency track at 92 (20 bytes each, a share), multiplied in.
- `int anim_moves(const Anim *a, int seq);`
  1 when animation `seq` moves any bone: some track has more than one
  moment in it (or in a global sequence it uses). A model whose standing
  animation doesn't move is drawn as made, never bent (most rocks).
- `void anim_pose(const Anim *a, int seq, uint32_t ms, uint32_t global_ms, float (*poses)[16]);`
  Every bone's pose for animation `seq` at `ms` into it (wrapped to its
  length) and `global_ms` since start (for bones on global sequences),
  as 4 x 4 matrices laid out as Raylib's (see 020-adt.h). `poses` holds
  nbones matrices. A track with no moments for this animation leaves
  that part of the bone at rest.
- `void anim_pose_facing(const Anim *a, int seq, uint32_t ms, uint32_t global_ms, float (*poses)[16], const float view[9]);`
  The same, with camera-facing bones (flags 0x8, 0x10-0x40) turned to face
  the camera. `view` holds, in the model's own space, three unit vectors:
  toward the camera, the camera's right, and its up (9 floats). A facing
  bone keeps its pivot where its pose put it; its own x axis (the side
  the original's billboard squares face) points at the camera.
- `int anim_has_billboards(const Anim *a);`
  1 when any bone faces the camera (the model must be posed per copy).
