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
| Window + input + audio | raylib 5.x      | Bundles window, input, audio, GL loading, font + texture + mesh helpers in one zero-dep library. We use raylib for primitives and drop down to its low-level `rlgl` module when we need direct GL. |
| Crypto                 | OpenSSL libcrypto | SRP6 (BigNum), RC4 (legacy cipher; explicitly enabled), HMAC-SHA1. |
| MPQ archives           | StormLib        | The de-facto MPQ implementation. No reason to rewrite. |
| Zlib compression       | miniz           | Header-only, lighter than zlib for our use. |
| Math                   | own (linmath-style) | We need exactly what the renderer + simulation use; small enough to keep in-tree and tune. |
| XML (FrameXML)         | own pull-parser | FrameXML uses a tiny subset of XML; libxml2 is overkill and a heavy dep. |
| Lua VM                 | LuaJIT          | See above. |

## Graphics API

OpenGL 3.3 core profile, accessed through raylib.

- raylib's default backend is GL 3.3 core; that's our target.
- For high-level work (textures, sprites, fonts, 2D camera, default shaders)
  we use the `Raylib.h` API directly.
- For the hard cases (terrain texture splatting, M2 skinning UBOs, particle
  vertex pulling, custom render passes) we use `rlgl.h` — raylib's
  thin-wrapper-over-GL layer that lets us issue raw `rlEnableShader`,
  `rlSetVertexAttribute`, `rlDrawVertexArrayElements` and friends.
- Vulkan was considered and rejected for v1: the cost of a 3.3.5a-era visual
  fidelity floor does not justify the bring-up cost. Re-open the question if
  we ever do an HD asset replacement pass.

## Audio

raylib's audio module (`raudio`), with a thin categorization + 3D layer of
our own on top.

- raylib gives us decoded sound loading, music streaming (MP3 / OGG /
  XM / WAV), per-source pan / pitch / volume, and the mixing callback.
- It does **not** give us channel categories (music vs. ambience vs. SFX 3D
  vs. SFX 2D vs. dialogue, each with independent master volume) or real 3D
  positional attenuation. We add a small layer above raylib that does the
  3D math + category mixing math, then drives raylib's per-source controls.
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
  half of it to an engine deletes the interesting parts. raylib is a
  **library**, not an engine: it owns no scene graph, no asset pipeline,
  no main loop, no allocator. We keep all of those.
- **Python** — explicitly dispreferred per CLAUDE.md. Build glue stays in
  shell + Lua.
