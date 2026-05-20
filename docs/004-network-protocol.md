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
