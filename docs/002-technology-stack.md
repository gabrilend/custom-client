# 002 — Technology Stack

Picks below are what the project is built with. Each line records *why*;
when the why no longer applies, the choice is open to revisit.

Rewritten 2026-10-07 for the merge of three projects into this one
(wow-chat-2026's custom client, this project's plans, and
world-edit-to-execute; wow-chat-2026 issue 173v). The working code that
arrives from wow-chat-2026 is the ground truth for what the stack already
is; where this document used to plan something else, the change and its
reason are in the last section.

## Language

| Layer | Language | Why |
|-------|----------|-----|
| Engine (both games) | C11 | Portable, predictable ABI, talks to OpenGL, sockets and audio with no marshalling layer. The working client is already C; world-edit-to-execute's client moves to C too, "so they fit in the same engine" (owner, 2026-10-07). |
| Scripting: interface, addons, game rules | LuaJIT 2.1 | The 3.3.5a addon ecosystem is Lua 5.1 plus Blizzard's extensions; LuaJIT is 5.1-compatible and fast. "We can use lua for scripting of course" (owner, 2026-10-07): Lua is the scripting language, never a second engine. Its `ffi` is not exposed to addons ([006](./006-ui-and-addons.md)). |
| Build | bash | "bash is easier for me. We can always call cmake inside of bash" (owner, 2026-10-07). One script calls the compiler directly ([008](./008-build-system.md)). |
| Tools | Lua (LuaJIT), bash | The house languages. No Python. |

No C++. One language at module boundaries, faster builds, one ABI.

## Libraries

| Concern | Library | Notes |
|---------|---------|-------|
| Window, input, OpenGL loading, audio | raylib 5.x | One small library with no engine in it: no scene graph, asset pipeline, main loop or allocator. The working client uses raylib 5.1 and drops to its `rlgl` layer and the OpenGL functions its loader exposes (texture arrays, compressed textures, anisotropy) where raylib has no call. |
| MPQ archives | **our own reader** | "We use our own extracter" (owner, 2026-10-07). Written from the format's public description; reads the 3.3.5a chain with its locale and patch archives, kept thread-safe for the reading-ahead thread. Writing archives (replacement packs, the stock-client seal) is a planned issue of its own. |
| Compression | the system's zlib | What the reader already uses. |
| Crypto (networking) | OpenSSL libcrypto | SRP6 (big numbers), RC4, HMAC-SHA1 for the 3.3.5a protocol ([004](./004-network-protocol.md)). |
| Threads | POSIX threads | The reading-ahead thread; locks where it shares the archive reader and the server's heights. |
| Math | our own | Exactly what the renderer and the mover need; raylib's `raymath` where it fits. |
| XML (FrameXML) | our own pull parser | FrameXML uses a small subset of XML. |
| Lua VM | LuaJIT | Above. |

## Graphics

OpenGL 3.3 core, through raylib.

- Hand-written GLSL 330 shaders (ground, models, water, sky, particles).
- The graphics card does the per-point work: models are bent by their
  bones on the card, the ground's pictures sit in one texture array,
  compressed pictures go to the card as stored (DXT). Measured and
  recorded in [005](./005-rendering-pipeline.md).
- Vulkan was considered and rejected: the 3.3.5a look doesn't need it.
  Revisit for a high-detail art pass.

## Audio

raylib's audio module, with a layer of our own for channel categories
and 3D positions ([007](./007-audio-system.md)). Shared by both games.

## Platforms

- **Tier 1:** Linux x86-64 (development).
- **Tier 2:** Windows x86-64.
- **Tier 3:** macOS, off the critical path.

## Toolchain

- gcc ≥ 12 or clang ≥ 16.
- `-std=c11 -Wall -Wextra -Werror` from day one: warnings are errors.
- Three checked builds besides the plain one: address-checked (`--asan`,
  memory used after freeing or past its end), race-checked (`--tsan`,
  two threads touching memory with no lock between), and the test program
  (`--test`).

## What we are not using

- **C++, Rust, Zig:** see Language.
- **A game engine** (Godot, Unity, Unreal): the design problem is the
  point; handing half of it to an engine deletes the interesting parts.
- **Python:** dispreferred by the house rules.

## Changed from the earlier plan (2026-10-07)

| Was planned | Now | Why |
|-------------|-----|-----|
| CMake, wrapped by shell scripts | one bash script calling the compiler | The owner's choice; the working client already builds this way in about 15 seconds |
| StormLib for MPQ | our own reader | The owner's choice; ours is built, tested and thread-safe |
| miniz | the system's zlib | What the working reader uses |
| One engine for the WoW world, WC3 maps hosted | one engine in C for both games, Lua scripting for both | Owner, 2026-10-07: "The goal is to be able to play both games seamlessly in the same binary, and to re-use as much of the code as we can between them" |
