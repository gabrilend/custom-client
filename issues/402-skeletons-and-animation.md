# 402 — Skeletons and Animation

*Came from: wow-chat-2026 173i (her animation, completed there) and 173j
(figures standing around, completed there), moved 2026-10-07.*

## Current behavior

Built:
- **Reading** (`src/023-anim.c`): a model's bones (parents, pivots,
  flags, camera-facing bones), its animations (ids, variations, lengths,
  speeds, blend times, aliases followed), the tracks of each bone (moves,
  turns, stretches, with global sequences), and the animations kept in
  `.anim` files beside the model; picture-position animations and
  animated colours.
- **Posing:** a bone's pose at any moment, blended between recorded
  moments; parent times local.
- **Bending on the graphics card:** each point's four bones and weights
  go to the card once; each frame only the bone poses go, as a one-row
  float picture; the shader bends (`041-figure.c`, `042-doodads.c`).
  Camera-facing bones turned per copy.
- **Choosing her animation** (`src/030-moves.c`): stand with fidgets,
  run, walk (walk toggled), walk backwards, the turning shuffle, strafing
  with her body turned, jump start, loop and landing, fall, swimming;
  moving animations paced by the speed the keys ask for, so she runs on
  the spot against a wall; changes blended.
- **Figures** (`src/041-figure.c`): any animated model (a character, a
  creature display, an item) posed and bent each frame; bystanders cycle
  their animations.
- Tests: her skeleton reads, she stands on the ground at her height, the
  run loops, the choices pick the right animation at the right pace.
- **Warcraft III's models posed** (step 4, 2026-11-04; `src/069-mdx-anim.c`,
  the C twin of world-edit-to-execute's `src/assets/anim.lua`, in the
  reader library): for any frame of any sequence of a model read by
  401's MDX reader, every node's matrix (moves, turns, stretches about
  its pivot; ways 0 to 3, turns along the sphere; global sequences on
  the thing's own clock), the matrix groups its points follow as the
  drawing's bones (a matrix each, laid out as M2's poses are), each
  unit's alpha, and where each attachment point is. Its choices are
  under Decisions below. Not yet drawn: no drawing part reads a WC3
  model yet (401).

Checked (WC3's posing):
- `tests/044-test-client.c`, "WC3's models posed" (25 checks, before the
  archives, so they run without the game's files): a model written by
  the test, a skeleton four deep with one way each (a straight line,
  way 0, hermite, bezier, each at a value only that way gives), a
  helper on a global sequence (a quarter turn at clock 100, the same at
  500), an attachment, a billboarded bone, groups shared and repeated;
  posed at and between keys, before Stand and past its end, a point
  bent by its group's average; a sequence it hasn't refused by name;
  then this client's own rules, which the Lua lacks: parents round in a
  loop cut once, a chain seventy deep kept whole.
- `tests/070-mdx-anim-matches-lua.lua` (LuaJIT): the Lua (through
  gpu.lua's build with stand-ins, as 062 does) and the C on the same
  models at many moments (every key in a sequence, half a frame and a
  frame either side, twelve steps through it, before it and 250 ms past
  it, on five clocks when anything follows a global sequence), compared
  bit for bit: every node's matrix, every group's, every unit's alpha,
  every attachment point, and the rig's order, parents, pivots and
  groups. Models made by the script: every way on every track, global
  sequences (one of length 0), NaN, repeated and unordered keys, keys a
  frame outside a sequence, a later kind of node taking an id, a group
  with no nodes, with a member no node has and with one member twice,
  a geoset past the Lua renderer's 128 groups, a geoset with fewer
  group numbers than points, nine deep; and two the C is meant to
  differ on (a parent loop, seventy deep), counted as decided.
  2026-11-04: 7 of 7 as expected, 810 poses and 144 attachment points,
  198949 values compared, every one identical. The same with the
  library built with the address and undefined-behaviour checkers, and
  the C test block clean under them. The comparison was tested by
  planting bugs in a copy (each way's weights, the shorter way round,
  the sphere's threshold, the global clock and its bounds, way 0's
  hold, NaN keys, the pivot, the parent's move, the groups' average,
  the layer's alpha, which geoset animation counts, which kind wins an
  id, the order's ties, unordered keys halved, a key just outside a
  sequence, the attachment's pivot, a member no node has, a turn not
  made unit length): each is caught; the loop and depth rules, which
  the comparison leaves to 044, are caught there.

Still needs WC3's own files (none exist here): `luajit
tests/070-mdx-anim-matches-lua.lua` over every `.mdx` in `war3.mpq` and
`War3x.mpq`, run when world-edit-to-execute's
`wc3-installs/frozen-throne/` holds them, on the owner's machine. Then,
by the bridge's rule (world-edit-to-execute's
`docs/wow-client-bridge.md`), anim.lua's posing can retire in favour of
`libwreaders.so`.

Measured against the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md), section 5,
filled 2026-10-16), where this differs from the original:
- **Which moving animation:** chosen here from the keys (back, walk
  toggled, else run). The original chooses by a fixed order of eight
  rules (note 5.3): swimming strafes, swimming, flying paths, backward,
  stealthed (stealth walk), **11 yards a second or more (sprint)**,
  above twice the walk speed (run), else walk. On the ground, strafing
  has no animation of its own there.
- **Blending:** here a straight-line mix over the new animation's blend
  time, or 150 ms when it has none. The original mixes along an S-curve
  (3t² − 2t³), and keeps a blend under way when the outgoing animation
  still carries more than half the weight, so a quick change doesn't
  snap (note 5.4). What it does for a blend time of 0 isn't said.
- **A missing animation:** here only the model's own aliases are
  followed (8 hops). The original follows the animation-data table's
  fallback link by link, stopping on a loop, a self-link or a missing
  record, then uses stand, or "closed", or the first animation; each
  link can mark it reversed (flag 16) or frozen (flag 32) (note 5.4).
- **Repeats** a random whole number of times between the model's least
  and most for that animation; **variations** picked by frequency out of
  32768 (here only for the stand's fidgets); **secondary animations**
  (the upper body) at most 75% of the weight.
- **Combat stance** (note 5.8): not built here. Chosen by the main-hand
  weapon, else the off-hand weapon: one-handed weapons, daggers, fist
  weapons and fishing poles "ready, one-handed"; two-handers "ready,
  two-handed" (one-handed if anything is in the off hand); polearms,
  staves and spears "two-handed long"; everything else, and no weapon,
  "ready, unarmed".
- **Jumping and landing** (note 5.8): this client plays jump start, loop
  and land. The original lands with jump-end only when standing still;
  moving forward or sideways faster than twice walking speed, it plays
  "land while running"; otherwise no landing animation at all. A
  knockback plays the fall animation; taking off into flight plays jump
  start, and stopping flight plays "land while running". The jump and
  land sounds play with them ([1003](./1003-sounds-in-the-world.md)).

## Intended behavior

Every model moves as the original moves it, and the same layer plays
WC3 models' animations with WC3 timing (walk speed matching, damage
point timing, decay: the WC3 behaviour layer of Phase 4), so one
animator serves both games.

## Suggested implementation steps

1. The original's choices (docs/057 notes 5.3 and 5.4; link back to them
   when done): the eight-rule choice of moving animation by speed and
   state; the fallback chain through the animation-data table with its
   reverse and freeze marks; the S-curve blend that keeps a blend under
   way; repeats and weighted variations for every animation; secondary
   animations capped at 75%.
2. Tests: a speed of 11 yards a second picks sprint, 10.9 run, twice
   walk speed exactly picks walk; a model lacking an animation lands on
   its table fallback; a fallback chain that loops ends on stand; two
   changes 50 ms apart don't snap.
3. Combat stance and the landing choice (note 5.8). Tests: landing
   while running at run speed plays "land while running"; landing while
   walking plays none; landing standing plays jump end; a two-handed
   sword with a shield in the off hand gives "ready, one-handed".
   Emote animations still wait on docs/057. Weapons, sheathing and
   riding are in [404](./404-characters-and-equipment.md).
4. **Built 2026-11-04** (above; the run over `war3.mpq` waits for the
   owner's machine). WC3 animations through the same posing (with
   401's MDX reader): the C twin of world-edit-to-execute's
   `src/assets/anim.lua` (docs/009, the second crosswalk). Not yet:
   billboarded nodes facing the camera (WC3 does; anim.lua turns them
   with their parent, and the twin does as the Lua until the Lua
   retires; 023's `anim_pose_facing` is the model for it), the
   "don't inherit" node flags, geoset colours and picture animations
   (both read by 061, posed by neither).
5. The WC3 behaviour layer: walk speed matching, attack damage points,
   death and decay.

## Decisions

Step 4's posing of WC3 models, decided 2026-11-04 in background work
(the owner asked for questions to be answered and recorded rather than
waited on). Each keeps anim.lua's answers wherever the two can agree;
any can be revisited.

1. **A matrix group is the drawing's bone.** In WC3 a point belongs to
   one matrix group of its geoset, a list of nodes, and follows the
   average of their matrices; M2's points hang from up to four bones by
   weights. So each group becomes one matrix (laid out as `anim_pose`'s
   poses, for the same bone picture) and each point follows one at full
   weight. The other way round, nodes as bones with each point weighted
   over its group's, fails twice: a group can have more than four
   nodes, and weights in 255ths can't give a third or a fifth exactly,
   which the Lua's average does.
2. **Groups with the same nodes are one group, model-wide.** The Lua
   (and its renderer) keeps each geoset's groups apart; here the same
   list of nodes in the same order is kept once (the same arithmetic,
   so the same bits), so fewer matrices go to the card. A point's
   group is 16 bits; a model with more than 65536 different groups is
   refused (`W_ERR_FULL`). The figures' and doodads' drawing puts a
   bone number in one byte, so a WC3 model past 256 groups needs that
   widened when the drawing takes WC3 models (401, 404).
3. **Group 0 is the empty one**, a matrix that changes nothing (the
   Lua's for a group whose nodes are all missing). Points that can't be
   posed follow it and stand as made, as the Lua leaves a geoset with
   no skin standing: a geoset with no groups, fewer group numbers than
   points, or a point naming a group past the geoset's.
4. **No limit of 128 groups to a geoset.** That is world-edit-to-execute's
   renderer's (3 vectors a group in its shader's uniforms), not WC3's;
   this client sends a bone picture. So such a geoset is posed here and
   stands there; the parity check counts it as decided. Geosets with
   nothing drawn are posed too (their points are in the list anyway).
5. **Parents round in a loop, and chains past 64.** The Lua's depth
   walk has a guard of 64 and memoises as it unwinds, so in both cases
   its answer depends on the order Lua's `pairs` visits the nodes, and
   in a loop it reads a parent's matrix before posing it (last frame's,
   or zero). Here the depth is the true one, and a loop is cut once,
   deterministically: walking up from the lowest id that leads into it,
   the node whose parent the walk already passed has none. Both are
   damaged files; the parity check counts them as decided.
6. **The same arithmetic, in doubles, in the same order**, so the two
   agree to the bit (checked: every one of 198949 values). Only the
   group matrices and alphas become floats, at the end, as the Lua's
   pose string does. Where the Lua caches the keys inside a sequence's
   bounds per track, this finds them each time, by halving when a
   track's keys are in order (the usual case) and by reading them all
   when not, which gives the Lua's answer either way. One branch of the
   Lua's (`around`'s "repeated keys") can't be reached with a frame
   that is a number; it is kept for likeness.
7. **The frame is on the model's timeline** (the sequence's start plus
   the time into it), as anim.lua's `pose_at` takes it, not time into
   the animation as `anim_pose` does: WC3's keys are on one timeline,
   and past a sequence's end its last keys hold rather than wrap.
   Global sequences take the thing's own clock (milliseconds since it
   was made), as M2's take `global_ms`.
8. **The rig is read only while posing**, and the caller holds the
   matrices (as `anim_pose`'s caller holds the poses), so one rig poses
   many copies, on many threads (the ceramic stations, 110e).
9. **The playheads aren't twinned here.** anim.lua's `state`, `play`,
   `step` and `pose` (a variant picked by rarity with `math.random`,
   picked again each loop; a non-looping sequence held at its end;
   poses rounded to 20 ms and cached, 4096 to a model) are choosing and
   stepping, which is step 5's WC3 behaviour layer, with this issue's
   step 1 rules beside it; the pose cache is the drawing's choice. The
   twin is `pose_at` and the attachment points of `pose`; the parity
   check reaches the latter through the Lua's playhead, at frames and
   clocks that are whole 20 ms steps.
10. **Billboards and "don't inherit" as the Lua**, for now: the twin is
    held to anim.lua first, and changes once anim.lua retires (step 4,
    above). A billboarded node's flags are in the model (061) for then.

## Related

- Posing runs as stations on the ceramic engine: [110e](./110e-per-frame-work-as-stations.md).
- **Blocks:** [1003](./1003-sounds-in-the-world.md) (the model events that fire sounds), [811](./811-spell-visuals.md) (casting, wound and death animations).

- [401](./401-m2-reader.md), [404](./404-characters-and-equipment.md).
