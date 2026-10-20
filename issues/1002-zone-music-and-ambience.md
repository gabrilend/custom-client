# 1002 — Zone Music and Ambience

*Written 2026-10-16 from the clean-room checklist's note 9.1
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 10's capstone sketched for "music + ambience streaming".*

## Blocked by

- Sound playback itself ([docs/007](../docs/007-audio-system.md): raylib's
  audio, streams, the volume categories).
- [303](./303-wdt-and-streaming.md)'s area under the player (which zone and
  sub-area she stands in), and the day and night of
  [306](./306-light-and-sky.md).

## Blocks

- [1008](./1008-phase-demo.md) ("music switches by zone, ambience layers
  correctly").

## Current behavior

The client is silent. No music or ambience is chosen or played.

## Intended behavior

Music and ambience change where, when and how fast they do in the
original (note 9.1):

- **Choice:** eleven layers (zone, sub-area, intro music and the rest),
  each with a day and a night choice; the highest layer with music for
  the time of day wins.
- **Intro music** only after its "next allowed" time, and never cut by
  another intro.
- **Silence between tracks:** a random wait between the zone-music
  record's shortest and longest silence, keeping the chosen music during
  the wait; none with `Sound_ZoneMusicNoDelay`.
- **Changes:** 4 s out and 4 s in; from silence, a 0.01 s start.
- **Stopped by** music from an addon script, camera or scene music, a
  movie, or `Sound_EnableAllSound` / `Sound_EnableMusic` off.
- **Ambience** loops; a change fades in over 5 s; crossing the water's
  surface switches at once.
- **Volumes** start at master 1.0, effects 1.0, music 0.4, ambience 0.6.

## Suggested implementation steps

1. The layer table filled from the area under the player and the time
   of day; the winning layer.
2. The music player's states: playing, fading, silent-waiting, stopped
   by something else.
3. Ambience with the water switch.
4. Tests (window-free, with time driven by the test, sound off):
   - stepping into a sub-area with its own music starts a 4 s change;
   - a track ending starts a wait inside the record's silence range;
   - an addon's music stops zone music until it ends;
   - going under water changes ambience in the same frame.

## Open questions

None.

## Related

- [docs/007](../docs/007-audio-system.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 9.1,
  [1003](./1003-sounds-in-the-world.md),
  [1001](./1001-the-options-menu.md) (the sound panel's settings).
