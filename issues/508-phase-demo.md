# 508 — Phase 5 Capstone Demo

## Current behavior

Phase 5 not started. No socket code, no auth handshake, no crypto.

## Intended behavior

A headless demo that:

- Reads auth host, port, account, password from `input/client.ini`
  (passwords for dev only — never committed).
- Performs the SRP6 auth handshake against a local AzerothCore.
- Retrieves the realm list, prints it.
- Connects to the first realm.
- Performs the world auth handshake.
- Sends `CMSG_CHAR_ENUM`, prints the returned character list.
- Holds the connection for 60 seconds while logging every received
  opcode (just the name + size; payload decoding is a later phase).
- Cleanly disconnects.

## Suggested implementation steps

To be expanded into per-issue tickets (501–507) when Phase 5 begins.
Sketch:

- 501 — TCP socket layer + connection state machine.
- 502 — SRP6 over OpenSSL BIGNUM (matches AzerothCore constants).
- 503 — Auth-server packet codec (1-byte opcode framing).
- 504 — World-server packet codec (2/4-byte opcode, size-in-stream).
- 505 — Wrath-era RC4 + HMAC-SHA1 keying + header crypt.
- 506 — Opcode table + name map.
- 507 — Network-thread architecture + SPSC packet rings.
- 508 — This capstone demo.

## Acceptance

- `./run-phase-demo.sh` → choose 5 → connects against a local
  AzerothCore, prints character list, exits 0.
- No opcode-decode warnings during the 60-second window.

## Related

- Independent of Phases 2–4; can run in parallel with them.
- Blocked by: Phase 1.
