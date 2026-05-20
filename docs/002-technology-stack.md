# 002 — Technology Stack

Picks below are the defaults the project is being built against. Each line
records *why* — when the why no longer applies, the choice is open to revisit.

## Language

| Layer       | Language      | Why                                                  |
|-------------|---------------|------------------------------------------------------|
| Client core | C11           | Portable, predictable ABI, lets us speak directly to GL / sockets / SDL with no marshaling layer. |
| Addons + UI | LuaJIT 2.1    | The whole 3.3.5a addon ecosystem is Lua 5.1 + Blizzard extensions. LuaJIT is fully 5.1-compatible, has a tracing JIT, and supports `ffi` (which we deliberately do **not** expose to addons — see [006](./006-ui-and-addons.md)). |
| Build       | CMake ≥ 3.20  | Industry default for C graphics work; integrates with every IDE. Wrapped by `scripts/build.sh` so command-line use stays one verb. |

We do **not** use C++. The codebase stays in C for ABI simplicity, faster
compile times, and one fewer language to reason about at module boundaries.

## Vendored / linked libraries

| Concern                | Library         | Notes |
|------------------------|-----------------|-------|
| Window + GL context    | SDL3            | Stable as of 2024. One library covers window, input, audio, threading, timers. |
| OpenGL function loader | glad2           | Generated against 3.3 core profile. |
| Crypto                 | OpenSSL libcrypto | SRP6 (BigNum), RC4 (legacy cipher; explicitly enabled), HMAC-SHA1. |
| MPQ archives           | StormLib        | The de-facto MPQ implementation. No reason to rewrite. |
| Zlib compression       | miniz           | Header-only, lighter than zlib for our use. |
| Math                   | own (linmath-style) | We need exactly what the renderer + simulation use; small enough to keep in-tree and tune. |
| XML (FrameXML)         | own pull-parser | FrameXML uses a tiny subset of XML; libxml2 is overkill and a heavy dep. |
| Lua VM                 | LuaJIT          | See above. |

## Graphics API

OpenGL 3.3 core profile.

- Available everywhere the original client runs and then some.
- Plenty for terrain + M2 + UI; we are not pushing modern-AAA techniques.
- Vulkan was considered and rejected for v1: the cost of a 3.3.5a-era visual
  fidelity floor does not justify the bring-up cost. Re-open the question if
  we ever do an HD asset replacement pass.

## Audio

SDL3's audio callback driving a hand-rolled mixer.

- 3D positional math lives in the scene layer; the mixer just consumes
  pre-attenuated PCM.
- Music is stream-decoded from MPQ; SFX are decoded once and cached.

## Platform targets

- **Tier 1**: Linux x86-64 (development platform).
- **Tier 2**: Windows x86-64 (where the original client ran).
- **Tier 3**: macOS — kept off the critical path; revisit when Tier 1/2 are
  feature-complete.

## Toolchain expectations

- gcc ≥ 12 or clang ≥ 16.
- `-std=c11 -Wall -Wextra -Wpedantic -Werror` from day one. Warnings are
  errors; this is a CLAUDE.md rule, not a stylistic flex.
- AddressSanitizer + UBSan builds are first-class; CI runs both.

## What we are **not** using

- **C++**, **Rust**, **Zig** — see language table above.
- **A game engine** (Godot, Unity, Unreal). The whole point of this project
  is the design problem of the original client's responsibilities — handing
  half of it to an engine deletes the interesting parts.
- **Python** — explicitly dispreferred per CLAUDE.md. Build glue stays in
  shell + Lua.
