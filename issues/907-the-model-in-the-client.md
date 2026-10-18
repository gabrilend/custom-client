# 907 — The Model in the Client

*Written 2026-10-17, when wow-chat-2026 replaced Ollama with llama.cpp
and named this client as one of the three places the model can run
(wow-chat-2026 issue 916d, "in-client"). The owner: "It should be bundled
using a library running either on the server, on a separate program
running on the user's computer, or in the custom-client."*

## Blocked by

- The world connection (Phase 5, not yet written as issues): the
  requests arrive as chat packets.
- Chat (this phase, not yet written as issues): the chat decoder that
  sees addon messages.
- [110](./110-the-ceramic-shape.md): generation runs as a station, not on
  a render thread.
- [1001](./1001-the-options-menu.md): the player turns it on.

## Blocks

- wow-chat-2026 916d's in-client placement, and through it the bots'
  chat (916k), guidance (916h) and the mirror companions (918) for any
  player who lends their GPU.

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
- **Request.** A 3.3.5a chat packet holds about 250 bytes, so a prompt
  arrives in numbered pieces: `req <request> <piece> <of> <text>`, and a
  grammar the same way under `gram`. The client joins pieces by request
  number; a request whose pieces stop arriving is dropped after a
  timeout, with a line in the log saying how many pieces came.
- **Reply.** `ans <request> <piece> <of> <text>` back to the server, and
  `err <request> <sentence>` when generation failed (model not loaded,
  context full, the grammar rejected by the library).
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
5. The settings: on or off, model file, slots, layers on the card
   ([014-settings](../src/014-settings.h), [1001](./1001-the-options-menu.md)).
6. Tests in `tests/044-test-client.c`:
   - the joiner: pieces out of order join into the right prompt; a
     missing piece drops the request after the timeout and says how many
     arrived;
   - the splitter: a 1,000-byte reply becomes pieces that each fit one
     packet and join back to the same bytes;
   - with a model file present: a one-word prompt produces at least one
     token, and with a grammar of `root ::= "yes" | "no"` produces exactly
     one of the two.
7. A picture: the frame time chart with a reply in progress, kept in
   `development-history/`.

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
- **The player's own bots only, or anyone's?** Should a player's GPU
  think only for the bots in their own party, or for any bot the server
  assigns to it?

## Related

- wow-chat-2026 issues 916 (mod-soren-chat), 916d (the model runner, the
  server half of this), 916e (workers), 916f (an in-client worker that
  stops answering is retired), 918 (the mirror companions: the player's
  GPU as a fourth seat).
- [001](../docs/001-architecture-overview.md) (who holds the truth,
  threads, handles), [002](../docs/002-technology-stack.md) (the library
  row), [110](./110-the-ceramic-shape.md) (stations).
