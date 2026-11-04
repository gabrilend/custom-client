/*
 * 061-mdx.h (issue 401 step 2; the C twin of world-edit-to-execute's
 * src/parsers/mdx.lua and the model part of its src/assets/gpu.lua) -
 * Warcraft III's models
 *
 * For a general audience: a Warcraft III model (.mdx) is one file holding
 * everything about a unit, a tree or a building: its pieces ("geosets":
 * corner points, which way each faces, where each sits on its picture,
 * triangles), its pictures, how each piece is painted ("materials", each
 * a stack of layers: picture, how it blends, whether light falls on it),
 * its animations ("sequences": stand, walk, attack, death, each a stretch
 * of the model's timeline in milliseconds), and the skeleton that moves
 * it ("nodes": bones and helpers, each hanging from a parent and turning
 * about its pivot point). This reads the whole file into one structure.
 * Its drawable part is the same model structure the game's own models
 * fill (022-m2.h), so one renderer draws both games' models (docs/001,
 * "One engine, two games"); what only Warcraft III has (its skeleton,
 * sequences, animated values) is kept beside it, read for issue 402.
 *
 * Layout (version 800, the one WC3 and The Frozen Throne ship; the fields
 * mdx.lua reads for version > 800 are read the same way): "MDLX", then
 * chunks, each a four-letter tag and a byte count. Read here: VERS, MODL,
 * SEQS, GLBS, TEXS, MTLS (with LAYS), GEOS (VRTX NRMS PTYP PCNT PVTX GNDX
 * MTGC MATS, then UVAS with UVBS), GEOA, BONE, HELP, ATCH, EVTS, CLID,
 * TXAN, PIVT, and the nodes of LITE, PREM, PRE2 and RIBB (their own
 * settings are skipped: lights are 310's, particles and ribbons 403's).
 * Any other chunk (CAMS, cameras, among them) is passed over and only its
 * tag is kept. Records with a size of their own are read to that size.
 * Animated values are tracks: a tag (KGTR, KMTA ...), the key count, the
 * way between keys (0 none, 1 linear, 2 hermite, 3 bezier: the last two
 * carry an in and an out tangent a key), a global sequence (-1 none),
 * then each key's frame and value.
 *
 * Into the drawable model (decided in issue 401 step 2, with reasons):
 *   points    every geoset's points in one list, geoset after geoset
 *             (MdxGeoset.vertices says where each geoset's are); normals
 *             the file doesn't give face up (0, 0, 1); the first set of
 *             picture positions in u, v and the second in u2, v2; further
 *             sets are skipped. Axes as the file has them (+x front, +y
 *             left, +z up, as M2's); WC3's units are not turned into yards
 *   pieces    one a geoset, its id the geoset's number; its triangles
 *             the geoset's faces of kind 4 (triangles; WC3 uses no other)
 *             cut to a whole number of triangles, as model point numbers
 *   units     one a geoset and material layer, in that order (a geoset
 *             whose material is missing gets one plain layer: opaque, the
 *             first picture, full alpha); none for a geoset with no
 *             points, more than 65535, or fewer than three corners
 *             (gpu.lua's rule). Each unit:
 *               texture, tex[0]  the layer's picture (tex[0] -1 when
 *                                there's no such picture)
 *               blend            the filter mode as it is: WC3's 0 none,
 *                                1 transparent, 2 blend, 3 additive,
 *                                4 add alpha, 5 modulate, 6 modulate 2x
 *                                are M2's 0 to 6, one for one
 *               flags            shading: 1 unshaded -> 0x1 unlit,
 *                                32 unfogged -> 0x2, 16 two sided -> 0x4,
 *                                64 no depth test -> 0x8, 128 no depth
 *                                set -> 0x10
 *               uvset[0]         the layer's coordinate set (0 or 1; a
 *                                later set draws with the first), -1 for
 *                                shading 2, sphere environment mapping
 *               uvanim[0]        its texture animation (TXAN), -1 none;
 *                                moving_uv when it has one
 *               alpha            its layer's alpha at the start of Stand
 *                                (KMTA, else the static value) times its
 *                                geoset animation's (KGAO), as gpu.lua
 *                                draws a model unposed: 0 hides death
 *                                and alternate parts until 402 poses it
 *               layers 1, shader 0, colour and fade -1 (WC3's animated
 *               tints are its own tracks, kept on the layer and the
 *               geoset animation)
 *   pictures  kind 0 and its path for a picture named in the file;
 *             M2_TEXTURE_WC3 + n (022-m2.h) for replaceable picture n
 *             (1 team colour, 2 team glow, 11 cliff, 31-37 trees ...),
 *             its path kept as the file has it; flags 1 wrap across,
 *             2 wrap down. Which picture stands in for a replaceable one,
 *             and what to draw when a picture is missing, is the
 *             drawing's choice (gpu.lua's own table and grey; 404's team
 *             colour), not the reader's.
 */
#ifndef W_MDX_H
#define W_MDX_H

#include "013-errors.h"
#include "022-m2.h"

#include <stddef.h>
#include <stdint.h>

/* the kinds of node, in mdx.nodes' order (a later kind wins an id two share) */
typedef enum { MDX_BONE, MDX_HELPER, MDX_ATTACHMENT, MDX_LIGHT, MDX_EMITTER, MDX_EMITTER2, MDX_RIBBON, MDX_EVENT, MDX_COLLISION, MDX_NODE_KINDS } MdxNodeKind;

typedef struct { uint32_t first, count; } MdxSpan;                    /* a run of one of the model's lists */
typedef struct { float radius, min[3], max[3]; } MdxExtent;           /* a bounding sphere's radius and a box */
typedef union  { float f[4]; uint32_t u[4]; } MdxValue;               /* a key's value: floats, or a whole number in u[0] */
typedef struct { int32_t frame; MdxValue value, in, out; } MdxKey;    /* in and out: tangents, for ways 2 and 3 */
typedef struct {
    char     tag[5];              /* "KGTR", "KMTA" ... */
    int      width;               /* floats a value: 1, 3 or 4; 0 a whole number (KMTF, KRTX) */
    uint32_t interpolation;       /* 0 none, 1 linear, 2 hermite, 3 bezier */
    int32_t  global;              /* its global sequence, -1 none */
    MdxSpan  keys;                /* into MdxModel.keys */
} MdxTrack;

typedef struct {
    char      name[81];
    uint32_t  start, finish;      /* milliseconds on the model's timeline */
    float     move_speed;
    int       non_looping;        /* 1 when the file says 1 */
    float     rarity;
    uint32_t  sync_point;
    MdxExtent extent;
} MdxSequence;

typedef struct {
    uint32_t filter_mode, shading, texture;
    int32_t  texture_animation;   /* into texture_animations, -1 none */
    uint32_t coord;               /* which set of picture positions */
    float    alpha;
    float    emissive, fresnel[3], fresnel_opacity, fresnel_team;   /* version > 800 only, else 0 */
    MdxSpan  tracks;              /* KMTA alpha, KMTF picture ... */
} MdxLayer;

typedef struct {
    int32_t  priority_plane;
    uint32_t flags;
    char     shader[81];          /* version > 800 only */
    MdxSpan  layers;              /* into MdxModel.layers */
} MdxMaterial;

typedef struct {
    MdxSpan   vertices;           /* its points in model.vertices */
    uint32_t  normals_given;      /* normals the file gave (those past the points are skipped; missing ones face up) */
    uint32_t  uv_sets;            /* sets of picture positions the file gave (the first two are kept) */
    uint32_t  uv_given[2];        /* positions given in each of those two */
    MdxSpan   face_types, face_counts, faces;      /* as the file has them (PTYP, PCNT: u32; PVTX: u16) */
    MdxSpan   vertex_groups;      /* each point's matrix group (GNDX, u8) */
    MdxSpan   group_sizes, group_indices;          /* each group's node count, then their object ids (MTGC, MATS) */
    uint32_t  material, selection_group, selection_flags;
    uint32_t  lod;                /* version > 800 only */
    char      lod_name[81];
    MdxExtent extent;
    MdxSpan   sequence_extents;   /* into MdxModel.sequence_extents, one a sequence */
} MdxGeoset;

typedef struct {
    float    alpha;
    uint32_t flags;
    float    color[3];
    uint32_t geoset;
    MdxSpan  tracks;              /* KGAO alpha, KGAC colour */
} MdxGeosetAnim;

typedef struct {
    int      kind;                /* MdxNodeKind: MDX_BONE ... */
    char     name[81];
    int32_t  id, parent;          /* object ids: pivots are by id; parent -1 none */
    uint32_t flags;
    MdxSpan  tracks;              /* KGTR, KGRT, KGSC; an attachment's KATV too */
    int32_t  geoset, geoset_animation;   /* bones; -1 for other kinds */
    char     path[261];           /* attachments */
    uint32_t attachment_id;       /* attachments */
    int      has_frames;          /* events with a KEVT */
    int32_t  event_global;        /* events: their global sequence */
    MdxSpan  event_frames;        /* events: into MdxModel.event_frames */
    uint32_t shape;               /* collision: 0 box, 1 plane, 2 sphere, 3 cylinder */
    int      npoints;             /* collision: 1 for a sphere, else 2 */
    float    points[2][3];
    float    radius;              /* collision: spheres and cylinders */
} MdxNode;

typedef struct {
    M2Model        model;         /* what is drawn (above) */
    int32_t       *unit_layer;    /* each unit's layer (into layers), -1 the plain stand-in */

    uint32_t       version;       /* 0 when the file has no VERS */
    char           name[81], animation_file[261];
    MdxExtent      extent;
    uint32_t       blend_time;
    MdxSequence   *sequences;          uint32_t nsequences;
    uint32_t      *global_sequences;   uint32_t nglobal_sequences;   /* durations, ms */
    MdxMaterial   *materials;          uint32_t nmaterials;
    MdxLayer      *layers;             uint32_t nlayers;
    MdxGeoset     *geosets;            uint32_t ngeosets;
    MdxGeosetAnim *geoset_animations;  uint32_t ngeoset_animations;
    MdxNode       *nodes;              uint32_t nnodes;              /* every kind, in the file's order */
    float        (*pivots)[3];         uint32_t npivots;             /* by object id */
    MdxSpan       *texture_animations; uint32_t ntexture_animations; /* each its tracks (KTAT, KTAR, KTAS) */
    MdxTrack      *tracks;             uint32_t ntracks;
    MdxKey        *keys;               uint32_t nkeys;
    MdxExtent     *sequence_extents;   uint32_t nsequence_extents;
    uint32_t      *face_types;         uint32_t nface_types;
    uint32_t      *face_counts;        uint32_t nface_counts;
    uint16_t      *faces;              uint32_t nfaces;
    uint8_t       *vertex_groups;      uint32_t nvertex_groups;
    uint32_t      *group_sizes;        uint32_t ngroup_sizes;
    uint32_t      *group_indices;      uint32_t ngroup_indices;
    uint32_t      *event_frames;       uint32_t nevent_frames;
    char          *chunks;             uint32_t nchunks;             /* every chunk's tag in order, 4 bytes each */
} MdxModel;

/* Reads a whole .mdx. W_OK, or with `why` (013-errors.h): W_ERR_CORRUPT
 * for a file that isn't MDLX, a chunk or record running past its end, a
 * sub-chunk that isn't the one expected, a track tag not known;
 * W_ERR_FULL when memory runs out. On failure `out` holds nothing to free. */
WErr mdx_read(const unsigned char *s, size_t len, MdxModel *out, WWhy *why);
void mdx_free(MdxModel *m);

/* The sequence named like `name` (C twin of mdx.sequence): first one
 * whose name, ignoring case, is `name` with only spaces and digits round
 * it ("stand" finds "Stand 2", not "Stand Ready"), else the first whose
 * name holds it, else the first; -1 when the model has none. `name` is
 * matched as plain text (the Lua takes it as a pattern; the same for
 * letters and spaces). */
int mdx_sequence(const MdxModel *m, const char *name);

/* The track tagged `tag` among `tracks`, or NULL (the last of two with
 * one tag, as the Lua keeps it). */
const MdxTrack *mdx_track(const MdxModel *m, MdxSpan tracks, const char *tag);

/* The node with object id `id` (C twin of mdx.nodes), -1 none. */
int mdx_node(const MdxModel *m, int32_t id);

/* A track's value at `frame` (C twin of mdx.sample, in doubles as the
 * Lua is): `def` (its width of values) when `t` is NULL or has no keys
 * there. With `bounded`, only keys from `first` to `last` count (the
 * playing sequence's). Before the first key its value, after the last
 * the last's; between, way 0 holds the earlier, the rest go in a straight
 * line (hermite and bezier as linear, as the Lua does), or along the
 * sphere for a rotation (`rotation`, 4 values). Fills `out` and returns
 * how many values (the track's width, 1 for whole numbers, or `ndef`). */
int mdx_sample(const MdxModel *m, const MdxTrack *t, double frame, const double *def, int ndef,
               int bounded, double first, double last, int rotation, double out[4]);

#endif
