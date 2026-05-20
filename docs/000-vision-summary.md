# 000 — Vision Summary

A from-scratch reimplementation of the World of Warcraft 3.3.5a client. It
loads the original proprietary assets, speaks the 3.3.5a network protocol to an
[AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) server, and
presents a player-facing experience indistinguishable from the official client.

The deliverable is **not a product**. When the proprietary assets are
eventually replaced with free assets, the server / client / asset trio becomes
fully open and the game can evolve outside the original IP constraints.

## What the client must do, end-to-end

- Read the proprietary asset archives (MPQ) and the file formats inside them
  (DBC, BLP, M2, WMO, ADT, WDT, DB2-precursors, animation files, sound files).
- Render terrain, world objects, characters, particles, full-screen overlays.
- Capture and rebind input, support gamepad-equivalents where the game allowed.
- Play positional sound and streamed music.
- Run the login → realm-select → character-select → enter-world flow.
- Speak the auth-server (SRP6) and world-server (RC4 + HMAC-SHA1) protocols.
- Host a [FrameXML](./006-ui-and-addons.md)-compatible UI runtime so existing
  3.3.5a addons load without modification.
- Send and receive chat, parties, raids, guilds, trade, mail, etc.

## What this project is about

The point is software design. Feature parity is the constraint that makes the
design problem real — every architectural decision has to survive contact with
a fully-specified, decade-tested set of player-visible behaviors. See
[001-architecture-overview.md](./001-architecture-overview.md) for the layering
choices that fall out of that constraint.

## Pointers

- Vision text: [notes/vision](../notes/vision)
- Roadmap split into phases: [009-roadmap.md](./009-roadmap.md)
- Documentation tree: [010-table-of-contents.md](./010-table-of-contents.md)
