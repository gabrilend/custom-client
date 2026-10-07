# 004 — Network Protocol

The 3.3.5a (build 12340) protocol has two distinct phases against two
different servers.

## Auth (logon) server — SRP6

- TCP, default port 3724.
- Wire framing: 1-byte opcode followed by a fixed (or first-field-length)
  body. No size prefix on this leg.
- Handshake:
  1. `CMD_AUTH_LOGON_CHALLENGE` — client sends username + locale + build.
  2. Server replies with N, g, salt, B, and a server-public-key.
  3. `CMD_AUTH_LOGON_PROOF` — client sends A and its proof M1.
  4. Server returns M2; both sides now share a session key K.
  5. `CMD_REALM_LIST` — client requests realm list, server returns
     [name, address:port, population, char-count, timezone] per realm.

SRP6 is implemented with OpenSSL `BIGNUM`. Constants (N, g, k) are the
3.3.5a-era values — these are baked into a header, not configurable.

## World server — RC4 + HMAC-SHA1

- TCP, port varies per realm.
- Wire framing: 2-byte big-endian size + opcode (client→server uses 4-byte
  opcode; server→client uses 2-byte). The size field itself is part of the
  encrypted stream after the handshake.
- Handshake:
  1. Connect; server immediately sends `SMSG_AUTH_CHALLENGE` with a 4-byte
     server seed.
  2. Client picks a 4-byte client seed; computes SHA1(account, 0, client-seed,
     server-seed, K). Sends `CMSG_AUTH_SESSION` with username, client seed,
     digest, build, and an addon checksum block.
  3. Server replies `SMSG_AUTH_RESPONSE`. From this point on, packet
     headers are encrypted/decrypted with RC4 keys derived from K via
     HMAC-SHA1 (Wrath-era keying — `0x...` constants for ServerKey and
     ClientKey are hard-coded).
- Application opcodes follow. Examples first phases need:
  - `CMSG_CHAR_ENUM` → `SMSG_CHAR_ENUM`
  - `CMSG_PLAYER_LOGIN` → world enter sequence
  - `MSG_MOVE_*` movement opcodes
  - `SMSG_MESSAGECHAT` / `CMSG_MESSAGECHAT`

## Who holds the truth (decided 2026-10-07)

Both games are **server-authoritative** on AzerothCore. The owner: "I
think we transition WETE to Azerothcore then", and, after weighing
lockstep: "I think lockstep isn't worth the implementation cost. Let's
just do server-authoritative and call it good. Maybe lockstep for WETE
LAN games or something...?"

- **The WoW world:** as the 3.3.5a protocol already works. The client
  moves its own character and sends where it is (`MSG_MOVE_*`), and asks
  for things (cast spell 133 on that target). The server works out every
  result (hits, damage, creatures' thinking, loot) and sends each client
  what is near it.
- **A WC3 map:** the client sends orders ("unit 12 moves to here"); the
  server runs the map (its triggers converted to server scripts) and
  sends each player what that player can see. A unit coming out of the
  fog arrives with its full state.
- **The world's shape only comes from the server.** Ground and doodads
  that change while playing are sent from the server; a client is never
  believed about them, and the server checks player positions against
  its own world.
- **Lockstep** (no server: every machine runs the whole game from the
  players' orders, and only orders are sent) may come later as a LAN mode
  for world-edit-to-execute's own engine. Not on AzerothCore.

### Considered and set aside (2026-10-07)

- **Orders sent player to player as well as to the server**, a player
  who withholds them caught by the others. Edge cases found: simulating
  only what a player sees breaks lockstep (things coming out of the fog
  have no known state, and causes cross the fog); a player can't notice
  what they can't see, while the server, holding every order, can check
  a digest of what each player received; lost packets look exactly like
  cheating; a player can lie about another; players' addresses are
  exposed to each other. Orders are a few bytes, so sending them directly
  saves little traffic; the only real gain is delay.
- **Switching a game to server-only when a player's packets go missing:**
  two systems to keep right and a hand-over mid-game, for a case server
  authority never has.
- **To measure:** under server authority every visible unit's changes are
  sent, where lockstep sent one order per click. Large WC3 battles
  (several hundred units) are where traffic is measured first.

## Frames: when timed things take effect (added 2026-09-23)

Open since 2026-10-07: this section was written for world-edit-to-execute's
own server (its W08). With WC3 maps now on AzerothCore, whether that
server is still built is to be decided in the merged planning.

With our own server (world-edit-to-execute W08), every timing-dependent
message carries the **frame** (uint32 simulation tick, 62.5 per second) at
which it takes effect: moves, path steps, hits, cast starts and landings, buff
expiries, deaths. The client keeps its own frame counter and:

- **future frame** → schedules the message and applies it when that frame arrives;
- **past frame** → fast-forwards it: applies it now, in the state it would have
  reached if it had arrived on time.

The owner's rule: hand-written, "we prioritize sane design over correctness".
The stock AzerothCore protocol has no frames; while talking to AzerothCore, the
client converts its millisecond times to frames with the same hand-written
per-packet rules the differential checker uses.

## Threading

The network thread owns the socket and the cipher state. It hands fully
decoded packets to the main thread via a single-producer/single-consumer
ring. Outbound packets go through the inverse ring. No locks on the hot
path — the rings carry pointers to arena-allocated buffers.

## Reconnection

Disconnect surfaces as a typed error to the main thread. The session is not
implicitly resumed — the player must re-trigger login. Reconnection within a
single world session (the `SMSG_REDIRECT_CLIENT` flow used for cross-realm)
is in scope for the social phase; v1 of the network layer can return a
"not implemented" error for it.

## Reference material

- AzerothCore source itself is the canonical reference — opcode IDs and
  packet structures are exactly what the server expects.
- See `src/server/game/Server/Protocol/Opcodes.cpp` in AzerothCore for the
  full opcode table.

Implementation lives in Phase 5: [009-roadmap.md](./009-roadmap.md).
