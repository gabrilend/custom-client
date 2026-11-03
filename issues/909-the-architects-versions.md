# 909 — The Architect's Versions

*Written 2026-10-21, from wow-chat-2026 issue 919d (the Architect). The
owner: "it's part of the custom client, the architect, so make sure he
goes over there. Though, he's sort of a wow-chat-2026 specific feature, so
maybe he goes here?" He is split: the character, the memories and the
generation are wow-chat-2026's (919d); drawing each player's versions is
this client's, here.*

## Blocked by

- The asset resolver (`docs/012-asset-replacement-and-provenance.md`,
  built "when the first replacement exists"): a player's versions are a
  replacement pack, ranked first.
- The world connection (Phase 5) and chat (this phase): versions and
  the square's choices arrive from the server as messages.
- wow-chat-2026 919d: the server side that makes versions and keeps each
  square's arrival order.

## Blocks

- Nothing yet.

## Current behavior

Not started. The client draws one version of every asset, chosen by the
resolver for the whole session.

## Intended behavior

In the Architect, a player tells a character how a place was for them
("your memory is what matters") and gets **their own version** of it:
models, textures and shapes that look the way they remember, and work
exactly as the originals do (the server checks "functionally the same"
before sending; 919d). This client:

1. **Keeps each player's versions as their own pack.** The resolver
   already ranks a player's packs first ("priority is: player chosen,
   server supplied, blizzard original"). Versions arrive as archives, as
   every network-delivered asset does here, and are unpacked into the
   player's pack, each with a provenance record (which memory, which
   photographs, which generation route, the check's result).
2. **Draws a square's chosen versions for those without their own.**
   The owner: "Whichever player entered that square first is the one who
   decides, until they leave." The server tells the client, per map square
   (one terrain tile), whose versions to draw there; the client asks the
   resolver for that player's versions in that square (they arrive like
   the server-supplied assets, ranked second), and switches when the
   server says the decider changed. The switch happens out of sight where
   it can (an asset swapped while nobody is looking at it), so a house
   does not change its paint before the player's eyes.
3. **Counts what it drew.** The HUD and the phase demo already count
   assets by source; versions add two sources: "mine" and "the square's".

## Suggested implementation steps

1. The resolver's two new sources, with tests on a hand-made pack.
2. Receiving versions: the server message, unpacking, provenance records.
3. Per-square deciders: the server message, the resolver's per-square
   lookup, swapping out of sight.
4. A scripted test with two clients: the first in a square sees its own
   green house; the second sees the first's; the first leaves and the
   second sees the recipe's house again. Screenshots filed in
   `development-history/`.

## Open questions

1. The square's decider is told to every client in it. Should a player be
   able to keep their versions private (919d's open question 2)? The
   client side is the same either way; only what the server sends changes.

## Related

- wow-chat-2026 `issues/919d-the-architect.md` (the whole design)
- [907](./907-the-model-in-the-client.md): the model can run in this
  client, so a player's version can be generated on their own graphics card
- `docs/012-asset-replacement-and-provenance.md` (the resolver, packs, provenance)
