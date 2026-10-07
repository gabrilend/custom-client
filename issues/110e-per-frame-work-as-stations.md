# 110e — Per-frame Work as Stations

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

Every frame, on the main thread, one after another: the mover steps,
every animated model is posed (`anim_pose`, `anim_pose_facing` in
`042-doodads.c`; figures in `041-figure.c`), every particle emitter is
stepped (`particles_step`), what is in view is decided (`032-sight.c`),
and Lua runs.

## Intended behavior

Each of these is a station, fed by the tick and the input argument, and
writing its slice of the mailbox's buffer being written:

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

## Suggested implementation steps

1. Posing as a station over chunks; the bone data written into the
   mailbox record; the render thread only uploads the bone picture.
2. Particles likewise.
3. What is in view, per view.
4. The mover and the input argument.
5. Measure: 500 and 5,000 figures (515k's crowd sizes), frame time and
   render-thread time, against the single-thread frame; written into 110.

## Acceptance

- The crowd measured and recorded; the render thread's time per frame
  no longer grows with the number of figures posed.
- Pictures (`scripts/run --shot`) of a scene identical before and after.

## Related

- world-edit-to-execute 515a, 515h, 515k; [402](./402-skeletons-and-animation.md), [403](./403-particles.md).
