# 110e — Per-frame Work as Stations

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

*Paused with its parent, 2026-10-07: blocked until soramech has timers
and standing tasks ([110](./110-the-ceramic-shape.md)).*

Every frame, on the main thread, one after another: the mover steps,
every animated model is posed (`anim_pose`, `anim_pose_facing` in
`042-doodads.c`; figures in `041-figure.c`), every particle emitter is
stepped (`particles_step`), what is in view is decided (`032-sight.c`),
and Lua runs.

## Intended behavior

Each of these is a **standing task** (110; the owner, 2026-10-07: "Let's
build it now and see what happens"; later the same day, to be built in
soramech itself): it stays in existence, the clock
(110g) hands it in each tick, it writes only its slice of the mailbox's
buffer being written, and a flag skips it for a tick while it is still
running, so a slow machine updates less often but never queues up work:

- **Posing** in chunks of figures (515a measured that one task per unit
  costs half a frame; 8 to 64 units a task, or one batch per frame,
  doesn't). A figure's pose depends only on its model and its animation
  clock, so chunks never touch each other.
- **Particles** per emitter group. A particle that needs the ground
  reads the ground's entry in the asset table, which never changes once
  published, so no lock is needed: a tile being replaced is a new entry,
  and the old one stays readable until no state names it.
- **What is in view**, per view (each render thread's camera).
- **The mover** (the player's own prediction) and **Lua** as stations.

**What a station may see, by the original's frame order**
([docs/057](../docs/057-what-the-original-program-decides.md) note 11.1,
filled 2026-10-16). Stations run side by side, but the original's frame
has an order, and the interface and addons see its results. So the
order is kept as **what each station reads**, not as when it runs:

1. Timers, then keyboard and mouse, then **every packet waiting**, in
   arrival order, with the interface events each one causes fired as it
   is handled. A packet that arrives after that waits for the next frame.
2. Movement: transports first, then every moving unit, then the
   player's own queued inputs; then the game clock and the zone check.
3. The interface's per-frame update (anchors, every update script in
   strata, level and shown order, each frame's animations; note 1.4).
4. Then, just before the world is drawn: the camera's target,
   passengers, the camera, the projection, day and night, the sound
   listener, object effects, **every visible unit's animation**,
   missiles, the world's loading, smooth turning.

So an interface script reads units and the camera as **last frame drew
them**, plus this frame's movement; posing (step 4) comes after the
scripts, not before. Work of equal rank runs newest-registered first.

## Suggested implementation steps

1. Posing as a station over chunks; the bone data written into the
   mailbox record; the render thread only uploads the bone picture.
2. Particles likewise.
3. What is in view, per view.
4. The mover and the input argument.
5. The reading order above: Lua's stations read the mailbox record of
   last frame's poses and camera, and this frame's movement; posing and
   the camera write the record the next frame's scripts read. A test
   that a script reading a unit's position during an update sees the
   moved position, and the pose from the frame before.
6. Measure: 500 and 5,000 figures (515k's crowd sizes), frame time and
   render-thread time, against the single-thread frame; written into 110.

## Acceptance

- The crowd measured and recorded; the render thread's time per frame
  no longer grows with the number of figures posed.
- Pictures (`scripts/run --shot`) of a scene identical before and after.

## Related

- world-edit-to-execute 515a, 515h, 515k; [402](./402-skeletons-and-animation.md), [403](./403-particles.md).
