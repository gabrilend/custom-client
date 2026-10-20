# 403 — Particles

*Came from: wow-chat-2026 173o (particles), moved 2026-10-07.*

## Current behavior

Built (`src/024-particles.c`, window-free; run by `042-doodads.c`):
- Each emitter read from the model (header 296, 476 bytes each): its
  bone, position, picture grid, blend, shape (plane or sphere), animated
  speed, spread, gravity, lifespan, rate and area, and how each
  particle's colour, opacity and size change over its life.
- Particles born at the rate (times Particle Density, a setting), flown
  with speed, gravity and drag, aged, dying at the end of their life;
  drawn as squares facing the camera, from the picture's cell, blended
  as the emitter says, faded into the fog.
- Run only for copies within 150 yards of the camera.
- Seeded by where the copy stands and which emitter it is, so a picture
  taken twice is identical to the pixel (needed by the test scenes, 109).
- Tests: a torch's flame emitter reads; its sparks rise; Particle
  Density thins them; the same seed throws the same sparks.

Left: figures' particles (spells, weapon glows; none of today's figures
carries an emitter), ribbons (trails), spline emitters (run as planes),
wind and twinkle, lit particles (flag 0x1), torches compared with the
original up close, all particles in one batch (measured small so far:
docs/047, item 6).

## Intended behavior

Every model's particle and ribbon emitters run as the original runs them,
WC3 models' too, through one particle system.

## Suggested implementation steps

1. Figures' emitters (with the first spell or glowing weapon).
2. Ribbons.
3. Wind, twinkle, lit particles; spline emitters.
4. One batch for all particles when spells make them many.

## Related

- Stepping particles runs as stations on the ceramic engine: [110e](./110e-per-frame-work-as-stations.md).

- [401](./401-m2-reader.md), docs/047 (item 6).

## Blocks

- [811](./811-spell-visuals.md).
