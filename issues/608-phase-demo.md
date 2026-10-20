# 608 — Phase 6 Capstone Demo

## Current behavior

Phase 6 not started. The renderer and network layer exist but are not
joined; there is no login flow.

## Intended behavior

The full pre-game flow:

- Splash → login screen → enter credentials → realm select → character
  list → click "Enter World".
- All UI drawn in C (no Lua yet — Lua lands in Phase 7).
- Upon entering the world: spawn at the character's last known position,
  with the character's M2 standing in front of the camera in the
  Phase 3/4 world.
- HUD adds: server-reported latency, ping graph, current zone name.

## Suggested implementation steps

To be expanded into per-issue tickets (601–607) when Phase 6 begins.
Sketch:

- 601 — Splash + cinematic stub (silent video, or just a held frame).
- 602 — Login screen (background BLP, two input fields, login button).
- 603 — Realm list screen.
- 604 — Character list + create + customize + delete screens.
- 605 — Character model preview (uses Phase 4 M2 renderer).
- 606 — World enter sequence (loading screen, terrain pre-stream).
- 607 — In-world overlay (zone label, latency, FPS).
- 608 — This capstone demo.

## Acceptance

- A user can launch the client and reach in-world standing under their
  character without touching anything other than the GUI.

## Related

- **Blocked by:** Phase 4, Phase 5, [609](./609-losing-the-connection.md), [602](./602-login-results.md), [606](./606-the-loading-screen.md).
