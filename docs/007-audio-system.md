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
