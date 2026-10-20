# 007 — Audio System

A thin categorization + 3D-positional layer built on top of raylib's audio
module (`raudio`). raylib gives us per-source play/pan/pitch/volume and
streamed music decoding; we layer on the things it does not provide:
channel categories with independent master volumes, 3D attenuation, and
the SoundEntries.dbc lookup.

## Channels

| Channel       | Count cap | Notes                                                |
|---------------|-----------|------------------------------------------------------|
| Master        | n/a       | Final gain.                                          |
| Music         | 1 stream  | Zone music. Crossfades on zone change.               |
| Ambience      | 4 streams | Layered weather + zone ambience.                     |
| SFX 3D        | 32 voices | World sounds with positional attenuation.            |
| SFX 2D        | 16 voices | UI clicks, error bonks, ready-check ding.            |
| Dialogue      | 4 voices  | NPC voice-over (sparse in 3.3.5a but used).          |

## Sources

- **Streams** (music + ambience): raylib `Music` handles, fed from
  in-memory buffers we sourced out of the MPQ chain (raylib's loaders
  normally read from disk; we use `LoadMusicStreamFromMemory`).
- **One-shots** (SFX): raylib `Sound` handles, also loaded from memory.
  We keep an LRU cache around them since raylib does not.

## 3D model

The original's rules for this section are in the clean-room checklist,
[057](./057-what-the-original-program-decides.md) section 9 and note
11.2 (filled 2026-10-16); the lines below follow them.

- **Listener:** by default (`Sound_ListenerAtCharacter` = 1) 2 yards
  behind the focused character along its facing and 4 yards up, facing
  the way it faces. At the camera, facing where it faces, when that
  setting is off, no character is in focus, or a model camera is in use
  (note 11.2). (An earlier version of this page put it at the camera
  always.)
- Per-source position + (optional) velocity.
- **Distance:** loudness 1 up to the sound-entries record's minimum
  distance; beyond it, min ÷ (min + 4 × (distance − min)); over the last
  tenth before the cutoff it also fades in a straight line to 0, and is
  0 at the cutoff and beyond. A sound kit may carry its own maximum.
  This curve replaces the sound library's own rolloff. (An earlier
  version said "linear between min and max".)
- **Doppler:** off in effect, as far as is known: the original sets the
  sound library's Doppler scale to 1 (057's distance row) and, by an
  answer since withdrawn from 057, never gives a sound or the listener a
  velocity, so no pitch shifts. Not built. Confirm when 057 answers it
  again.
- **The player's own footsteps** play flat (not placed in 3D) at 65%
  loudness while the listener is at the character; everyone else's play
  in 3D at the foot. Which footstep sound is chosen by ground and
  creature: 057's footstep row.
- **Zone music and ambience** fades and silences: 057 note 9.1 (4 s out
  and 4 s in, ambience 5 s, none across the water's surface, random
  silence between tracks unless `Sound_ZoneMusicNoDelay`).
- **Voices:** `Sound_NumChannels`, default 32. What happens when all are
  busy isn't yet known.
- **Variation:** sound-entries flag 2048 varies loudness by up to ±0.15;
  flag 1024 varies pitch between 0.85 and 1.15.
- Stereo panning from the unit vector listener→source projected onto the
  listener's right axis.

## File → sound lookup

`SoundEntries.dbc` maps a sound entry ID to up to 10 candidate filenames
(picked randomly per play) plus volume, pitch, min/max distance. We respect
that table verbatim — it is the data the entire game's audio was authored
against.

## Threading

- raylib's `UpdateMusicStream` is called once per frame from the main
  thread; raylib's internal audio thread does the actual playback.
- Per-source state (position, attenuation result, target volume) is
  computed on the main thread once per frame and pushed into raylib via
  `SetSoundVolume` / `SetSoundPan` / `SetSoundPitch`.
- We do not run our own audio thread, and we do not allocate during
  per-frame audio updates.

## Phase plan

Phase 10. Earlier phases ship silent; the rendering / networking work does
not benefit from audio being present, and the audio interface stays small
enough to land late without rippling.

See [009-roadmap.md](./009-roadmap.md).

## Answered

1. ~~**Doppler: on or off?**~~ Off in effect (2026-10-16): no sound or
   listener is ever given a velocity, so the scale of 1 never acts. (The
   answer came from a 057 note that was later withdrawn, because it was
   written in reply to a question a builder session had put into 057,
   which builders must not edit; to be confirmed when 057 answers it on
   its own.) The question arose because 057's distance row gives the
   Doppler scale as 1, which in the sound libraries of that time usually
   means "on, at ordinary strength", against this page's earlier line
   that the original didn't use it.
