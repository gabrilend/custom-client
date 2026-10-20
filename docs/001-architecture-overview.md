# 001 — Architecture Overview

One native program in C, playing two games: the World of Warcraft 3.3.5a
world and Warcraft III maps converted by world-edit-to-execute. Lua is
its scripting language (the interface and addons; the games' rules run
on the server, where AzerothCore's mod-ale also speaks Lua), never a
second engine. Rewritten 2026-10-07 for the merge of three projects into this
one (wow-chat-2026 issue 173v, its answers 13 and 15 in particular).

## One engine, two games

The owner, 2026-10-07: "The renderers need to merge. We should be able to
load both model types and animations and particles... all visual stuff.
Also audio is shared ... The goal is to be able to play both games
seamlessly in the same binary, and to re-use as much of the code as we
can between them. The goal is to condense the two games into a single
project."

| Shared by both games | Each game's own |
|----------------------|-----------------|
| Drawing: ground, buildings, models of both formats, animations, particles, sky and light | The rules: WoW's are AzerothCore's; a WC3 map's are its converted triggers, run on AzerothCore |
| Audio | Controls: third-person movement and action bars; box select, orders and the command card |
| Reading files: archives, tables, pictures, models | The camera's style: behind her; top-down |
| Networking: the 3.3.5a protocol to AzerothCore | The interface's layout |
| Settings, input, the interface's runtime | |

The rules of either game never reach into drawing or audio, and drawing
never decides a rule. A game's rules hand the engine *what is where and
doing what*; the engine draws and plays it.

## Who holds the truth: the server

Both games are **server-authoritative** on AzerothCore (owner,
2026-10-07: "Let's just do server-authoritative and call it good").

- A player's client sends what the player asks for: its own movement (as
  the 3.3.5a protocol does), orders, spell casts.
- The server runs the game and sends each player what that player can
  see. A unit coming out of the fog arrives with its full state.
- **The world's shape comes only from the server.** Ground and doodads
  may change while playing (handles make that possible, below), but a
  change is sent from the server to clients, never accepted from a
  player. The server checks player positions against the world it holds,
  "so they can't just reshape the world and walk through locked doors"
  (owner, 2026-10-07).
- A lockstep mode (every machine running the whole game from shared
  orders, no server) may come later for world-edit-to-execute's LAN
  games. It is not planned for AzerothCore.

## Threads

**Decided 2026-10-07: the ceramic shape** ([110](../issues/110-the-ceramic-shape.md)).
The owner: "I'd prefer not to have any main thread at all, or if we do,
that it's only the rendering thread. Ideally we'd be able to have
multiple rendering threads for different openGL contexts." Every part's
work becomes a station on the ceramic engine's workers; the only other
threads are render threads, one per OpenGL context, which take the
newest state, upload finished assets and draw. The table below is the
shape as built today, until 110 lands.

| Thread | Does | State |
|--------|------|-------|
| **Main** | input, the frame's simulation, Lua, every OpenGL call | built |
| **Reading ahead** | reads the next tiles' files out of the archives and prepares their ground (points, light, blend pictures, unpacked pictures), so the main thread only sends them to the card | built (wow-chat-2026 173u) |
| **Network** | the server sessions, their ciphers, the packet codec, a queue of decoded packets | planned (Phase 5) |
| *(no thread of its own)* **Thinking** | a language model (llama.cpp) answering the server's prompts for its bots, one token per turn as a station on 110's workers, so it never holds a thread for seconds; the reply goes back to the server, which checks it | planned ([907](../issues/907-the-model-in-the-client.md)) |

The reading-ahead thread never touches OpenGL: it hands finished arrays
to the main thread. What it shares (the archive reader's memory, the
server's heights) is locked. A race-checked build (`--tsan`) checks it.

## Layers (low → high)

```
+----------------------------------------------------------------+
|  addons (Lua)  |  default interface (Lua)  |  WC3 interface (Lua)  |
+----------------------------------------------------------------+
|        scripting surface (C functions callable from Lua)        |
+----------------------------------------------------------------+
|  interface tree | game state | what is drawn | audio | settings |
+----------------------------------------------------------------+
|  renderer (OpenGL) | network session | input | mover, collision |
+----------------------------------------------------------------+
|  readers: archives | tables | pictures | models | ground | buildings |
+----------------------------------------------------------------+
|        raylib (+ rlgl) | zlib | OpenSSL | pthreads | LuaJIT       |
+----------------------------------------------------------------+
```

Each layer depends only on those below. Lua sees only the scripting
surface: never the renderer, the network thread, or raw file bytes.

**Window-free and drawing parts are kept apart.** The readers and the
game's arithmetic (preparing ground, the mover, collision, the settings
store) build without a window. That is what lets the test program run
them alone, the reading-ahead thread run them, and the reader library
(`libwreaders.so`) offer them to world-edit-to-execute. The drawing parts
take what the window-free parts made and send it to the card.

## Handles: each part's state in one place, many if needed

Decided 2026-10-07 (173v, answer 13): every part keeps its state behind
a **handle**, a pointer to a structure the part allocates in its init
call and frees in its free call. Every other call takes the handle.

```c
/* the shape every part follows */
WErr ground_init(const GroundPlace *place, Ground **out, WWhy *why);
WErr ground_load_tile(Ground *g, GroundPrep *prepared, AdtTile **kept, WWhy *why);
void ground_free(Ground *g);
```

- No part keeps state in its own file. Two grounds, two sets of doodads,
  two worlds can live in one program: a WoW map beside a WC3 map, the
  many headless test characters of the test scenes (wow-chat-2026 176a),
  a model preview beside the game.
- A part allocates what it owns and frees it; callers never reach inside.
- Lower parts expose headers; higher parts include them. Never upward.

(The working client kept one copy of each part's state inside its file;
it is converted as it moves. Type names keep the working code's style:
`Ground`, `MpqChain`, `Mover`.)

## Errors: a code to branch on, a sentence to read

Decided 2026-10-07 (173v, answer 13): "The W client's errors feel more
flexible because they allow the calling code to branch depending on
errors. Ours is more readable, but we could just create more verbose
error types and expand the list."

- A call that can fail returns a `WErr`, one code from one list in
  `errors.h` (`W_OK`, `W_ERR_NOT_FOUND`, `W_ERR_CORRUPT`, `W_ERR_FULL`
  ...). The list grows whenever a caller needs to tell two failures apart.
- It also fills a `WWhy`: the code again and a sentence saying exactly
  what failed and where ("the ground picture X is in none of the
  client's archives"), checked when written. The caller branches on the
  code and shows or logs the sentence.
- A good sentence says what didn't finish *and* what did, so the reader
  knows where in the work things stopped.

## Failure model

Fallbacks are warnings, and warnings are errors (house rule). The client
fails loud and early. When it does carry on (a picture missing from the
archives: the piece is left out), it says so on the terminal, by name,
every time, and the case is listed in an issue until it is handled
properly.

## Profiles: each server is a profile

Owner, 2026-10-07 (173v, answer 9): "even on the user's end, each
'server' is a profile for them, and could have different visual
settings". A player keeps one profile per server they play on: its
address, its own settings (video, interface, key bindings), and its own
assets (the game files it reads, later replacement packs). Assets are
read in place for now; installing them per profile comes later.

## The reader library

The window-free readers (archives, tables, pictures, models, ground
files, buildings) are also built as `libwreaders.so` with a plain C
header, built during the move (173v, answer 14). world-edit-to-execute
loads it from Lua through LuaJIT's FFI to convert maps and score
replacements: one implementation of each format, used by both projects.

## Pictures without a window

The client can draw a scene straight into a picture file with no window
open, so many scenes can be photographed in one run and compared with
stored references (planned; the test scenes, wow-chat-2026 176a).

## Related

- [002-technology-stack.md](./002-technology-stack.md): what each layer is built on.
- [004-network-protocol.md](./004-network-protocol.md): the sessions.
- [005-rendering-pipeline.md](./005-rendering-pipeline.md): the frame.
- [012-asset-replacement-and-provenance.md](./012-asset-replacement-and-provenance.md): where assets come from.
