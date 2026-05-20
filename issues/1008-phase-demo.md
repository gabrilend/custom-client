# 1008 — Phase 10 Capstone Demo

## Current behavior

Phase 10 not started. Client runs but is silent and lacks polish layers.

## Intended behavior

A 30-minute supervised session in which the client is, at a casual
glance, indistinguishable from the original 3.3.5a client:

- Audio: music switches by zone, ambience layers correctly, SFX fire on
  spells and footsteps, NPC voice-over plays.
- Full-screen effects: weather (rain in Stormwind), underwater tint,
  vignette on low health, "drowsy" overlay on stun.
- Settings UI: rebind a key, change resolution, change graphics quality,
  changes persist.
- Video options visible and effective.

This is the project's terminal capstone — the marker that v1 is done.

## Suggested implementation steps

To be expanded into per-issue tickets (1001–1007) when Phase 10 begins.
Sketch:

- 1001 — Audio categorization layer on top of raylib's audio module.
- 1002 — Music + ambience streaming.
- 1003 — 3D SFX with SoundEntries.dbc.
- 1004 — Weather effects (particles + post-process).
- 1005 — Full-screen overlay framework (vignette, tint, daze, drowsy).
- 1006 — Key binding UI (rebind, persist, scheme presets).
- 1007 — Settings panel + video options + persistent settings file.
- 1008 — This capstone demo.

## Acceptance

- The 30-minute session is recorded and shows feature parity at a
  casual-glance level.

## Related

- Blocked by: Phase 9.
- Final issue in the v1 plan.
