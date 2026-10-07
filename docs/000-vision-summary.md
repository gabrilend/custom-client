# 000 — Vision Summary

**The W client.** An open-source, from-scratch client for the World of
Warcraft 3.3.5a world that talks to an
[AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) server. It is
also the host for Warcraft III maps converted by the sibling project
`world-edit-to-execute` (its Phase W, "WoW Client Bridge"). It starts on
Blizzard's assets because those are the models we have, and it replaces them
one at a time until the whole game is legally distinct.

The name comes from world-edit-to-execute's Phase W, which designed the parts
this project and that one share. On 2026-09-23 the two designs were merged,
and this project's documents and issues were rewritten to the merged design.
The previous versions are in git history (commit `48811d0` and earlier).

**October 2026: three projects become one.** On 2026-10-07 the owner
decided to merge wow-chat-2026's working custom client (C on raylib: the
world of Ashenvale drawn from the game's own files, her moving by the
server's own rules, checked by its own test program) into this project, and to fold
world-edit-to-execute's client into it as well: "The goal is to be able
to play both games seamlessly in the same binary, and to re-use as much
of the code as we can between them. The goal is to condense the two games
into a single project." The working code is the start of this project's
`src/`; its design and the owner's decisions on it are recorded in
wow-chat-2026 issue 173v and carried into these documents:

| Decided 2026-10-07 | Where |
|--------------------|-------|
| One engine in C for both games; Lua for scripting only | [001](./001-architecture-overview.md), [002](./002-technology-stack.md) |
| Both games server-authoritative on AzerothCore; lockstep only maybe, later, for LAN | [004](./004-network-protocol.md) |
| Every part behind a handle; errors as codes that keep their sentence | [001](./001-architecture-overview.md) |
| Our own archive reader, the system's zlib, a bash build | [002](./002-technology-stack.md), [008](./008-build-system.md) |
| Blizzard's files are the placeholders; the resolver waits for the first replacement | [012](./012-asset-replacement-and-provenance.md) |
| Each server a player plays on is a profile, with its own settings and assets | [001](./001-architecture-overview.md) |
| Source files numbered in reading order | [010](./010-table-of-contents.md) |

## What changed from the first design

| First design (May 2026) | W client (September 2026) |
|-------------------------|---------------------------|
| "In all respects exactly the same as the official client" | **Behaves** like the official client: its behaviour is the spec. **Looks** steadily less like it as art is replaced |
| Loads the proprietary assets | Loads every asset through a **resolver**: replacement pack first, then the owner's Blizzard client files, then a visible placeholder. Each borrowed asset is counted |
| One mode: the WoW world | Two modes: the **WoW world** (AzerothCore) and **WC3 map mode** (converted WC3 maps with RTS controls: box select, command card, top-down camera) |
| Reading layer private to this client | Reading layer (MPQ chain, DBC, BLP, later M2) built here once as a C library. world-edit-to-execute calls it from Lua through LuaJIT's FFI |
| Assets replaced "eventually" | Replacement is a measured process: every replacement has a **similarity score** against the original and counts as replaced only past a distinctness threshold ([012](./012-asset-replacement-and-provenance.md)) |

## The end goal

In the owner's words (2026-09-23): "Eventually, we want to have a complete,
legally distinct game, reverse engineered (with these llm-transcripts as proof)
from the best, yet legacy and deprecated, version of the most powerful,
impactful, and successful example of the genre."

Everything is replaced over time, not only models: animations, particle
effects, textures, sounds, fonts, interface art. Server data is a separate
question, recorded in [012](./012-asset-replacement-and-provenance.md).

## What the client must do, end to end

- Read the asset archives (MPQ), with loose-file folders layered on top for
  converted WC3 maps and replacement packs.
- Resolve every asset through the resolver and report where each came from
  (once replacements exist; until then, read Blizzard's files directly).
- Render terrain, world objects, characters, particles, full-screen overlays.
- Capture and rebind input; in WC3 map mode, RTS selection and orders.
- Play positional sound and streamed music.
- Run login → realm select → character select → enter world.
- Speak the auth-server (SRP6) and world-server (RC4 + HMAC-SHA1) protocols.
- Host a [FrameXML](./006-ui-and-addons.md)-compatible UI runtime so existing
  3.3.5a addons load without modification.
- Chat, parties, raids, guilds, trade, mail.

## Stock-client compatibility is a badge, not a requirement

The stock 3.3.5a client does not have to read our assets. A replacement that
does work in it (right formats, within its limits, and checked by loading it in
the stock client) earns a **"default client compatible"** seal. Most assets are
not expected to have it.

## What this project is about

The point is software design. The original client's behaviour is the fixed
specification that makes the design problem real. See
[001-architecture-overview.md](./001-architecture-overview.md).

## Pointers

- Vision text: [notes/vision](../notes/vision) (the owner's original words, unchanged)
- Asset replacement, scores, seal, provenance: [012](./012-asset-replacement-and-provenance.md)
- Roadmap: [009-roadmap.md](./009-roadmap.md)
- The WC3 side: `/mnt/mtwo/programming/ai-stuff/world-edit-to-execute/docs/wow-client-bridge.md`
- Documentation tree: [010-table-of-contents.md](./010-table-of-contents.md)
