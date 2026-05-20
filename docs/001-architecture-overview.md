# 001 — Architecture Overview

The client is a single native process written in C, hosting an embedded
[LuaJIT](./002-technology-stack.md) interpreter for the addon and FrameXML
surface only. The split mirrors the original 3.3.5a client: C owns
performance and hardware, Lua owns the UI behavior loop.

## Process model

One process. Three primary threads:

| Thread        | Responsibility                                                |
|---------------|----------------------------------------------------------------|
| **Main**      | Input, simulation tick, Lua VM, UI scripts, GL submission      |
| **Network**   | Auth + world TCP, crypto, packet codec, decoded-packet queue   |
| **Streamer**  | MPQ reads, decompression, GPU upload staging (PBO + fences)    |

The streamer never touches GL state directly — it stages bytes into
mapped buffers and the main thread issues the upload. This keeps GL on a single
context and avoids the shared-context dance.

Audio runs on whatever thread SDL3's audio callback chooses; mixing happens
in a pre-allocated ring buffer fed by the main thread.

## Layering (low → high)

```
+----------------------------------------------------------+
|   addons (3rd-party Lua)   |   FrameXML default UI (Lua) |
+----------------------------------------------------------+
|        FrameXML API surface  (C functions, Lua-callable) |
+----------------------------------------------------------+
|  UI widget tree | gameplay state | scene graph | audio   |
+----------------------------------------------------------+
|  renderer (GL) | net session | asset cache | input | log |
+----------------------------------------------------------+
|  MPQ | DBC | BLP | M2 | WMO | ADT | crypto | sockets     |
+----------------------------------------------------------+
|     SDL3   |   OpenGL 3.3   |   OpenSSL   |   StormLib   |
+----------------------------------------------------------+
```

Each horizontal slice only depends on the slices beneath it. Lua only sees
the FrameXML surface — never the renderer, never the network thread, never
raw asset bytes.

## Where C ends and Lua begins

**C** — anything that has a frame budget (rendering, decode, simulation),
anything that touches a syscall or a vendor library (sockets, GL, audio,
MPQ), and anything addons should not be allowed to influence (the network
session state machine, crypto, file I/O outside saved-variables).

**Lua** — the FrameXML XML+Lua UI tree, addon scripts, saved variables, and
the event-driven UI behaviors that Blizzard's own UI is built on. Lua runs
sandboxed: no `io.*`, no `os.*` beyond `time`/`date`, no `package.loadlib`,
no debug library writes. See [006-ui-and-addons.md](./006-ui-and-addons.md).

## Module boundaries

- **No header includes one direction.** Lower layers expose `.h` files;
  higher layers include them. No upward includes, ever — broken with a build
  rule, not a convention.
- **No singletons.** Each subsystem returns a `*_t` handle from an
  `_init()`; everything else takes the handle. Makes the wiring legible and
  testing trivial.
- **Allocation at the boundary.** A subsystem allocates what it owns and
  returns pointers; callers never reach in to free private members.

## Failure model

Per CLAUDE.md: fallbacks are warnings, warnings are errors. The client
fails loud and early. A missing asset, a malformed packet, a corrupt MPQ —
each surfaces as a typed error that the caller decides how to render to the
user (modal dialog, in-game chat, log-only). There is no silent retry path.

## Related

- [002-technology-stack.md](./002-technology-stack.md) — what each layer is
  built on.
- [005-rendering-pipeline.md](./005-rendering-pipeline.md) — main-thread
  frame structure.
- [004-network-protocol.md](./004-network-protocol.md) — network-thread
  state machine.
