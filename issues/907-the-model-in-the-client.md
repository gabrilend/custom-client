# 907 — The Model in the Client

*Written 2026-10-17, when wow-chat-2026 replaced Ollama with llama.cpp
and named this client as one of the three places the model can run
(wow-chat-2026 issue 916d, "in-client"). The owner: "It should be bundled
using a library running either on the server, on a separate program
running on the user's computer, or in the custom-client."*

*It is also the issue the owner asked for on 2026-10-04 (wow-chat-2026
916o, its Origin): "could we add an issue file for the custom client that
packages llama.cpp or similar into the client? then, we can "shard" or
"distribute" llm compute tasks to all the players who wanted to
contribute." 916o built that plan around a separate companion program,
because the stock client cannot run a model; this client can, so here the
companion program is not needed. 916o's rules and its gizmo apply as
written.*

## Blocked by

- The world connection (Phase 5, not yet written as issues): the
  requests arrive as chat packets.
- Chat (this phase, not yet written as issues): the chat decoder that
  sees addon messages.
- [110](./110-the-ceramic-shape.md): generation runs as a station, not on
  a render thread.
- [1001](./1001-the-options-menu.md): the player turns it on.

## Blocks

- wow-chat-2026 916d's in-client placement, and through it, for any
  player who lends their GPU: the bots' chat lines (916k, 916l), that
  player's own plain-text commands to their own bots (917c), and a seat
  for one of the mirror companions (918). Never the guidance that steers
  bots' tactics (916h): see "What the server sends here", below.

## Current behavior

Nothing exists. The client cannot run a language model, and nothing in
it listens for the server asking it to.

## Intended behavior

A player on this client may lend their computer to the server's bots:
the server sends a prompt, the client runs it through a language model
on the player's own GPU (or processor), and sends the text back. The
server still decides everything ([001](../docs/001-architecture-overview.md),
"Who holds the truth"): the client returns words, and the server checks
them exactly as it checks an answer from its own model.

### What the server sends here

A player's machine is a *contributed* place (wow-chat-2026 916o): the
server cannot vouch for it, and the player could change the client. So
the server sends it only work whose worst answer is harmless:

- **Chat lines** for any bot (916k, 916l): one sentence, said once.
  Before sending, the server scrubs the prompt of player names, account
  data and anything typed in a whisper (916o's scrubber), because the
  player will see it. The reply passes the server's full filter whatever
  the grammar said, because this client could ignore the grammar.
- **This player's own commands** for their own bots (917c): built only
  from their words and their state, so whatever comes back they could
  have typed themselves. Never another player's.
- **Never guidance** (916h), which sets the tactics of bots other
  players fight beside; and, as proposed in 916k, never a bot's origin
  story, which is stored and reused forever.

A client whose replies keep failing the server's checks is quarantined
like a dead machine (916f). The trust rule lives on the server (916e's
separate inboxes); this client does nothing to enforce it and cannot
weaken it.

### The gizmo

916o's in-game window, drawn by this client itself rather than as an
addon: what the player's machine is doing right now, abstracted to
who-and-where ("an orc in Stranglethorn is talking to a night elf", no
character or player names), the text as it streams, and the dials:

| Shows | Type | From |
|-------|------|------|
| tokens per second | number | the station's own count over the last few seconds |
| prompt length against the context window | two integers, as a gauge | the job record and the context size |
| time to first token | milliseconds | the job record |
| jobs done this session | integer | the thinker handle |
| recent job lengths and speeds | bar chart and sparkline | the last N job records |

and two sliders the player owns: jobs at once (integer; 0 = paused, the
`slots` field) and the longest job they'll accept (tokens). The scene
summary arrives with each request, written by the server; the client
never sees more of the conversation than the prompt itself.

### The library

**llama.cpp** (<https://github.com/ggml-org/llama.cpp>): a library that
loads one model file and produces text from it, one token at a time.

- Its interface, `llama.h`, is plain C, so the client's own code stays
  C11. The library itself is written in C++ and is built separately
  (CMake, called from the build script, as [008](../docs/008-build-system.md)
  allows); the client links the finished `libllama` and its `ggml`
  libraries. That brings the C++ runtime (`libstdc++`) into the process
  as a dependency, though not into our code (open question below).
- Built at a pinned upstream tag, the way wow-chat-2026 916c and the
  sibling project neocities-modernization (`scripts/build-deps.sh`) build
  it. GPU support is a build choice: CUDA for an NVIDIA card (the
  development machine's 1080 Ti needs CUDA 12.x, which still includes
  its Pascal generation), Vulkan for any card, or processor only.
- The model is one `.gguf` file (llama.cpp's single-file format: the
  weights, the tokenizer and the prompt template together), chosen by
  the player, kept in `assets/models/` (git-ignored, like the rest of
  `assets/`). A 1B model with four-bit weights is about 0.8 GB.

### The thinker (one handle)

The part's state lives behind one handle, like every other part
([001](../docs/001-architecture-overview.md), "Handles"):

| Field | Type | What it holds |
|-------|------|---------------|
| model | pointer to llama.cpp's model | the loaded weights, read-only once loaded |
| contexts | array of pointers to llama.cpp contexts, one per slot | each slot's working memory (its key/value cache: every token already read, `context length × layers × width` numbers) |
| slots | integer | how many requests run at once; the player's setting, default 1 |
| jobs | array of job records, `slots` long | the request each slot is working on |
| pieces | table of partial requests, keyed by request number | prompts still arriving in pieces |

A job record: `request` (unsigned 32-bit integer, the server's request
number), `prompt_tokens` (array of 32-bit token numbers),
`produced` (byte buffer, the text so far), `max_tokens` (integer),
`grammar` (string, GBNF text or empty), `sampler` (pointer to llama.cpp's
sampler chain: temperature, then the grammar).

### One token per turn

Generating a reply takes seconds; a frame takes 16 milliseconds. So
generation never runs as one long call. It is a station on the ceramic
engine's workers ([110](./110-the-ceramic-shape.md)) that does **one
step** each turn: on the first turn it feeds the prompt's tokens in one
batch; on each later turn it asks the sampler for one next token, feeds
it back, and appends its text. A reply of 80 tokens is 80 turns. No
worker is held for longer than one step, and a render thread never
waits for it.

The GPU is shared with drawing. llama.cpp's GPU work goes through its
own CUDA or Vulkan context, separate from the OpenGL context the render
threads draw with, so they do not interfere in code, but they share the
card's time and memory. How many of the model's layers go on the card
(`n_gpu_layers`) is a setting; the frame time with and without a reply
in progress is measured and recorded in
[047](../docs/047-optimization-checklist.md).

### Talking to the server

Requests and replies travel as **addon messages**: chat packets with a
prefix and a payload, the 3.3.5a mechanism addons use to talk. The
server side is in wow-chat-2026 916d (it sends with ALE's
`Player:SendAddonMessage` and listens with ALE's addon-message event).

- **Offer.** After entering the world, if the player turned it on, the
  client sends `SOREN` / `offer <slots> <model file name>`. The server
  then creates a worker for this player (wow-chat-2026 916e). Turning it
  off sends `withdraw`.
- **Request.** The server's side checks no length when it sends an
  addon message (ALE's `SendAddonMessage` writes the length into the
  packet as it is; read in wow-chat-2026's `source-beta/`, 2026-10-17),
  so a prompt and its grammar can arrive whole:
  `req <request> <scene summary> <text>` and `gram <request> <text>`.
  The client's chat decoder accepts addon messages up to a limit of its
  own choosing (open question below). The piece format
  (`<piece> <of>`) is kept for prompts longer than that limit; the
  client joins pieces by request number, and a request whose pieces stop
  arriving is dropped after a timeout, with a line in the log saying how
  many came.
- **Reply.** `ans <request> <piece> <of> <text>` back to the server, and
  `err <request> <sentence>` when generation failed (model not loaded,
  context full, the grammar rejected by the library). This direction is
  bound by the server's rules, read from its source:
  - the server **drops any chat message over 255 bytes without a word**
    (`ChatHandler.cpp`), addon messages included, so each piece, header
    and prefix included, is at most 255 bytes;
  - each addon message a client sends within one second of its previous
    one adds to a count, and at 100 the player is **muted for ten
    seconds**, their own chat included (`ChatFlood.AddonMessageCount`,
    `ChatFlood.AddonMessageDelay`, `ChatFlood.MuteTime`;
    `Player::UpdateSpeakTime`). A gap of a second resets the count. The
    client counts its own sends (its addons' too, since they share the
    budget) and waits out a one-second gap before the count would reach
    100. A one-sentence chat reply is one piece, so this matters only to
    a player contributing steadily.
- These messages are handled in C, below the interface layer. They are
  never shown in chat and never passed to addons as `CHAT_MSG_ADDON`, so
  an addon cannot read or forge them.

### Errors

Every failing call returns a `WErr` and fills a `WWhy`
([013-errors](../src/013-errors.h)): model file missing or unreadable,
library failed to load, context could not be allocated (with how much
memory it asked for), a request with too many tokens for the context.
Each error names what had already succeeded (model loaded? context made?
how many pieces?) so the reader knows where it stopped.

## Suggested implementation steps

1. The build: the bash step that clones llama.cpp at its pinned tag into
   `tmp/`, builds it with CMake, and installs `lib/` and `include/`
   where the client's build finds them; the client linked against it.
2. The thinker handle: load the model, make the contexts, free both.
3. The generation station: one step per turn, the sampler with
   temperature and grammar, the stop at an end token or `max_tokens`.
4. The piece joiner and splitter, and the five message kinds (`offer`,
   `withdraw`, `req`/`gram`, `ans`, `err`).
5. The settings: on or off, model file, slots, layers on the card,
   longest job accepted ([014-settings](../src/014-settings.h),
   [1001](./1001-the-options-menu.md)).
6. The sender's pacing: a count of this client's addon messages (its
   own and its addons'), and the one-second wait before it would reach
   the server's flood limit.
7. The gizmo window, drawn from the thinker handle and the job records.
8. Tests in `tests/044-test-client.c`:
   - the joiner: pieces out of order join into the right prompt; a
     missing piece drops the request after the timeout and says how many
     arrived;
   - the splitter: a 1,000-byte reply becomes pieces of at most 255
     bytes each, prefix and header included, that join back to the same
     bytes;
   - the pacing: 150 replies queued at once go out with a one-second gap
     before the hundredth message, and never 100 in a row closer than a
     second;
   - with a model file present: a one-word prompt produces at least one
     token, and with a grammar of `root ::= "yes" | "no"` produces exactly
     one of the two.
9. A picture: the gizmo and the frame time chart with a reply in
   progress, kept in `development-history/`.

## Open questions

- **The "No C++" line in [002](../docs/002-technology-stack.md).** Our
  code stays C, but linking llama.cpp puts a C++ library and its runtime
  in the process. Is that inside the rule (the rule is about our code)
  or does it need the owner's yes?
- **Which GPU backend to build by default:** CUDA (fast on the
  development machine, NVIDIA only) or Vulkan (any card, the platforms
  in 002's tiers)?
- **The test with a model file.** Downloading a model for every test
  run is large. Run that test only when a model file is configured, and
  say plainly in the test output that it did not run otherwise?
- **Whose model?** Does the player choose any `.gguf`, or does the
  server name the model it expects (so every in-client place answers in
  the same voice), with the client refusing to offer if it doesn't have
  it?
- **Whose bots?** Partly settled by 916o's rules ("What the server sends
  here"): chat lines for any bot, commands only for the player's own.
  Still open there (916o's second question): should a player's *nature
  prompt* shape only the characters their machine performs, only their
  own buddies, or everyone's?
- **How long an addon message does this client accept from the server?**
  The server sends any length; the stock client's limit is not known
  here. A generous limit (say 4 KB) lets a whole prompt arrive in one
  message; the piece format covers anything longer.
- **This route or 916o's broker?** Addon messages ride the game
  connection the client already has. 916o's companion program instead
  holds its own outgoing connection to a job broker, with no 255-byte or
  flood limits. Should this client use the broker too, so every
  contributed place looks the same to the server (wow-chat-2026 916d,
  Open Questions)?

## Related

- wow-chat-2026 issues 916 (mod-soren-chat), 916d (the model runner, the
  server half of this), 916e (workers and the two inboxes that keep
  guidance off players' machines), 916f (an in-client worker that stops
  answering is retired), 916o (player-contributed compute: the trust
  rules, the gizmo, the nature prompts), 917c (a player's own commands),
  918 (the mirror companions: the player's GPU as a fourth seat).
- [001](../docs/001-architecture-overview.md) (who holds the truth,
  threads, handles), [002](../docs/002-technology-stack.md) (the library
  row), [110](./110-the-ceramic-shape.md) (stations).
