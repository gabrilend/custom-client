# 007 — Audio System

A small hand-rolled mixer driven by SDL3's audio callback. The mixer is
intentionally simple: enough channels, enough attenuation modes, and enough
streaming bandwidth to replicate the original client's audio behavior, with
no DSP graph or middleware.

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

- **Streams** (music + ambience): MP3 / OGG decoded a frame at a time into
  a small ring buffer. Decoder runs in the SDL audio callback's frame, so
  it has to be fast — minimp3 / stb_vorbis fit.
- **One-shots** (SFX): decoded once at load, kept as int16 PCM in the SFX
  cache. The cache evicts LRU when over its memory budget.

## 3D model

- Listener position + orientation from the camera.
- Per-source position + (optional) velocity.
- Distance attenuation: WoW-style "min/max distance" model, linear
  between min and max.
- Doppler: off by default. The original game did not use it. Wire it in
  but leave it disabled.
- Stereo panning from the unit vector listener→source projected onto the
  listener's right axis.

## File → sound lookup

`SoundEntries.dbc` maps a sound entry ID to up to 10 candidate filenames
(picked randomly per play) plus volume, pitch, min/max distance. We respect
that table verbatim — it is the data the entire game's audio was authored
against.

## Threading

- The SDL callback is the only audio-producing thread.
- The main thread submits "play sound" messages onto a lock-free SPSC ring
  consumed by the callback.
- The callback never allocates.

## Phase plan

Phase 10. Earlier phases ship silent; the rendering / networking work does
not benefit from audio being present, and the audio interface stays small
enough to land late without rippling.

See [009-roadmap.md](./009-roadmap.md).
