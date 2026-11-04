/*
 * 069-mdx-anim.h (issue 402 step 4; the C twin of world-edit-to-execute's
 * src/assets/anim.lua, its posing) - Warcraft III's models posed
 *
 * For a general audience: a Warcraft III model moves by its skeleton. Each
 * node (a bone, a helper, an attachment point ...) hangs from a parent and
 * has three animated values: how far it moves, how it turns, how much it
 * stretches, all about its own pivot point. An animation ("sequence":
 * stand, walk, death) is a stretch of the model's one timeline, in
 * milliseconds. This works out, for any moment of any sequence, where
 * every node is (a matrix each), and from those the matrices the model's
 * points follow. In Warcraft III a point doesn't hang from bones with
 * weights, as the game's own models' points do; it belongs to one
 * "matrix group" of its piece, a short list of nodes, and follows the
 * average of their matrices. So here each matrix group is what the
 * drawing calls a bone: one matrix, and each point tied to one of them
 * at full weight. That is the same shape the game's own models are bent
 * with (023-anim.h: a matrix per bone, laid out the same, sent to the
 * graphics card as a bone picture), so one way of drawing serves both.
 * It also works out each drawn unit's opacity at that moment: pieces
 * come and go (a corpse, a weapon swapped) by their animated alpha.
 *
 * What is posed, as anim.lua poses it (decided in issue 402, with
 * reasons, where the two differ):
 *   order     every node of every kind, one per object id (the last of a
 *             kind list, later kinds winning: mdx_node's rule), parents
 *             before children: by depth, then by id. A node whose parent
 *             is itself or missing has none; parents that lead round in
 *             a loop are cut once: walking up from the lowest id that
 *             leads into it, the node whose parent was already passed
 *             has none
 *   a node    its move (KGTR, default 0), turn (KGRT, default none) and
 *             stretch (KGSC, default 1) at the frame, read from the keys
 *             inside the sequence's bounds (a global sequence's: its own
 *             loop, at the clock's place in it); before the first such
 *             key its value, past the last the last's; between, way 0
 *             holds the earlier, 1 goes in a straight line, 2 (hermite)
 *             and 3 (bezier) along their curves with the keys' tangents;
 *             a turn goes along the sphere (the shorter way) for any way
 *             but 0. A value with a NaN in it is taken as no change.
 *             matrix = parent's * move(pivot + move) * turn * stretch *
 *             move(-pivot), in doubles, as 3 x 4 rows
 *   a group   the average of its nodes' matrices (ids no node has are
 *             left out; none left: no change); groups with the same
 *             nodes in the same order are one group. Group 0 is always
 *             the empty one: points that can't be posed (their piece's
 *             groups don't cover its points) follow it and stay as made
 *   an alpha  each drawn unit's: its geoset animation's (KGAO, else its
 *             static alpha; the last such animation for its piece) times
 *             its layer's (KMTA, else its static alpha); 1 for NaN
 * Not done, as anim.lua doesn't: billboarded nodes (they turn with their
 * parent here; WC3 turns them to the camera), the "don't inherit" flags,
 * geoset colours, picture animations. Choosing and stepping sequences
 * (anim.lua's playheads) is the WC3 behaviour layer's (402 step 5).
 */
#ifndef W_MDX_ANIM_H
#define W_MDX_ANIM_H

#include "013-errors.h"
#include "061-mdx.h"

#include <stddef.h>
#include <stdint.h>

/* a node in posing order: what it hangs from and what moves it */
typedef struct {
    int32_t         id;            /* its object id */
    int32_t         parent;        /* its parent's place in MdxRig.nodes (always earlier), -1 none */
    int32_t         node;          /* into MdxModel.nodes */
    float           pivot[3];      /* by its id; (0, 0, 0) when the model has none for it */
    const MdxTrack *move, *turn, *stretch;   /* KGTR, KGRT, KGSC; NULL none */
    int             attachment;    /* 1 when an attachment has this id */
} MdxRigNode;

/* a model's rig: its nodes in posing order, the groups its points follow */
typedef struct {
    const MdxModel *m;                            /* the model it poses: kept, and must outlive the rig */
    MdxRigNode     *nodes;    uint32_t nnodes;    /* parents before children */
    MdxSpan        *groups;   uint32_t ngroups;   /* each group's run of members; groups[0] is the empty one */
    uint32_t       *members;  uint32_t nmembers;  /* places in nodes, group after group */
    MdxSpan        *geoset_groups;                /* per geoset: its matrix groups' runs in group_of (count 0: not posed) */
    uint32_t       *group_of;                     /* a geoset's matrix group k -> its group in groups */
    uint16_t       *point_group;  uint32_t npoints;   /* per point of the model (M2Model.vertices): the group it follows */
    int32_t        *unit_animation;  uint32_t nunits; /* per unit (M2Model.units): its geoset animation, -1 none */
    unsigned char  *sorted;                       /* per track of the model: 1 when its keys' frames never go down */
    int             on_clock;                     /* 1 when some node or alpha follows a global sequence */
} MdxRig;

/* The rig for a model read by mdx_read: its nodes in posing order, its
 * groups, each point's group, each unit's geoset animation. W_OK, or with
 * `why`: W_ERR_FULL when memory runs out or the model has more than 65536
 * different groups. On failure `out` holds nothing to free. */
WErr mdx_rig_build(const MdxModel *m, MdxRig *out, WWhy *why);
void mdx_rig_free(MdxRig *r);

/* Every node's matrix at `frame` (milliseconds on the model's timeline:
 * the sequence's start plus the time into it; past its finish the last
 * keys hold) of sequence `seq`, with `clock` (milliseconds the thing
 * drawn has existed) placing the global sequences. `world` holds nnodes
 * matrices, 3 x 4 rows (x' = w[0] x + w[1] y + w[2] z + w[3] ...), in
 * MdxRig.nodes' order; `alphas` (nunits floats) may be NULL. W_OK, or
 * W_ERR_NOT_FOUND when the model has no sequence `seq` (nothing filled). */
WErr mdx_rig_pose(const MdxRig *r, int seq, double frame, double clock, double (*world)[12], float *alphas, WWhy *why);

/* Every group's matrix from the nodes' (mdx_rig_pose's `world`), as
 * 4 x 4 matrices laid out as 023-anim.h's poses are (column by column:
 * a point p goes to m[0] p.x + m[4] p.y + m[8] p.z + m[12], and so on).
 * `poses` holds ngroups matrices: the bone picture the drawing sends. */
void mdx_rig_groups(const MdxRig *r, const double (*world)[12], float (*poses)[16]);

/* Where attachment `id` is in the pose: its node's matrix applied to its
 * pivot. 1 with `out` filled, or 0 when no attachment has that id or the
 * place isn't a number (anim.attachment's nil). */
int mdx_rig_attachment(const MdxRig *r, const double (*world)[12], int32_t id, double out[3]);

/* A point of the model bent by the groups' poses (mdx_rig_groups):
 * position (and facing, when `n_in` is given, kept unit length), as
 * 023-anim.h's anim_skin_point bends the game's own. */
void mdx_rig_skin_point(const MdxRig *r, const float (*poses)[16], uint32_t point, const float in[3], float out[3],
                        const float *n_in, float *n_out);

#endif
