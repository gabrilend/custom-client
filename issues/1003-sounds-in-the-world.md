# 1003 — Sounds in the World

*Written 2026-10-16 from the clean-room checklist's section 9 and note
11.2 ([docs/057](../docs/057-what-the-original-program-decides.md)), at
the number Phase 10's capstone sketched for "3D SFX with
SoundEntries.dbc".*

## Blocked by

- Sound playback itself ([docs/007](../docs/007-audio-system.md)).
- [106](./106-dbc-parser.md) (the sound-entries, terrain-type and
  footstep tables), [402](./402-skeletons-and-animation.md) (the model
  events that fire sounds), [207](./207-camera.md) (where the camera is).

## Blocks

- [1008](./1008-phase-demo.md) ("SFX fire on spells and footsteps").

## Current behavior

The client is silent. Nothing places, fades or picks a sound.

## Intended behavior

A sound in the world is as loud, as placed and as varied as in the
original:

- **Where the player hears from** (note 11.2): 2 yards behind the
  focused character and 4 up, facing its way, by default
  (`Sound_ListenerAtCharacter`); at the camera otherwise.
- **How loud by distance** (row "a 3D sound gets quieter"): 1 up to the
  record's minimum, then min ÷ (min + 4 × (distance − min)), faded in a
  straight line to 0 over the last tenth before the cutoff.
- **Footsteps** (row "footstep sounds"): the ground's terrain type and
  the creature's footstep number pick the sound from the footstep
  table, dry or splash, falling back to terrain type 0, else silence. The
  player's own steps flat at 65%; everyone else's placed at the foot.
  Which model event is a step: waiting on docs/057.
- **Wading** (note 2.3): a splash and a ripple each time the water's
  depth crosses 0.4 of body height.
- **Variation:** flag 2048 loudness ±0.15; flag 1024 pitch 0.85 to 1.15.
- **Voices** (row "at most this many sounds play at once", filled
  2026-10-16): two limits. All sounds: `Sound_NumChannels`, 32, with the
  loser chosen by priority (zone music and ambience 100, the player's
  own footsteps 115). The same sound: at most 3 copies of one
  sound-entries record at once; a fourth stops the quietest copy, except
  that other units' footsteps, looping sounds and some kits instead mute
  the extra copy, fading it over 0.5 s, and bring it back when a place
  frees up.
- **No Doppler:** the original never gives a sound or the listener a
  velocity, so no pitch shifts with motion (docs/007; its checklist
  source was withdrawn, so confirm when docs/057 answers it again). Not
  built.

## Suggested implementation steps

1. The listener's place each frame (it is worked out just before the
   world is drawn: docs/057 note 11.1, [110e](./110e-per-frame-work-as-stations.md)).
2. The distance curve, replacing the library's rolloff.
3. Footstep choice and the two ways of playing them; the wading splash.
4. Loudness and pitch variation.
5. Tests (window-free, sound off, loudness computed and checked):
   - with min 8 and cutoff 40, a sound 16 yards off is at 8 ÷ 40 = 0.2,
     one at 38 yards is faded by a further 0.5, one at 40 is silent;
   - the listener sits 2 back and 4 up from a character facing north;
   - grass under a creature with a footstep number picks that pair's
     dry sound, and one with no pair picks terrain 0's.

## Open questions

None of its own. Which kits mute rather than stop, and the priority
of each other kind of sound, wait on docs/057.

## Related

- [docs/007](../docs/007-audio-system.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) section 9,
  [1002](./1002-zone-music-and-ambience.md).
