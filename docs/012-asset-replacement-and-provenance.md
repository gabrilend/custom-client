# 012 — Asset Replacement and Provenance

How the W client moves from Blizzard's assets to its own, and how it keeps a
record of how each replacement was made.

## Today: Blizzard's files, as placeholders

The owner, 2026-10-07: "we don't have a replacement pack yet ... we only
have the blizzard ones for now, not even placeholders. We're just using
them as, well, placeholders." So the client reads every asset straight
from the owner's archives, as the working client did before the merge.
The resolver below is built when the first replacement exists; until
then every asset counts as borrowed.

Built ahead of that, 2026-11-04 (issue 105a, background work the owner
asked for): the resolver (`src/060-resolver.c`), the folder index and
the pack manifest's reader under it (`src/059-loose-folders.c`). No
drawing part asks it yet; they still read the archives directly, and
switch to it one at a time (204 for pictures first).

## How players share assets

The owner, 2026-10-07: "A plain folder of files would be converted into
a compressed archive when received. When we use our system to pass the
files back and forth over the network, we will work with the compressed
archive version, but we shouldn't require them to, and do the conversion
as part of the input process." So a player may hand in a folder of files
or an archive. A folder is packed into an archive when it is taken in,
and only archives travel over the network. Packing needs the archive
writer (planned: [105](../issues/105-mpq-reader.md)).

## The resolver (built 2026-11-04, `src/060-resolver.h`)

Every asset request (model, texture, animation set, particle definition,
sound, font, interface image) goes through one function that asks five
places in order:

```
 logical path ("Creature\Wolf\Wolf.m2")
      │
      ├─ 1. replacement packs (loose folders, ours)          → source = "pack"
      ├─ 2. assets the server delivered (2026-10-09)         → source = "server"
      ├─ 3. overlays (loose folders: converted WC3 maps)     → source = "overlay"
      ├─ 4. the owner's Blizzard client (MPQ chain)          → source = "blizzard"
      └─ 5. placeholder (magenta checker / box / beep)       → source = "placeholder"
```

Overlays (issue 105a's second step) sit after the server's assets: the
server holds the truth about the world it runs (docs/001), so where a
local folder and the server's delivery hold the same path, the server's
wins. A player's own pack outranks both.

The player's own packs outrank the server's assets (the owner,
2026-10-09: "A player's own replacements are higher priority than the
server's"). For an asset both have, the server's comes before the original game's
("priority is: player chosen, server supplied, blizzard original", the
same day). They arrive over rmail, through the asset browser (open
question 4).

- A "blizzard" answer is allowed and counted. It is the normal state today.
- A "placeholder" answer is a warning; strict mode makes it an error.
- The HUD and every phase demo show the counts: how many assets drawn this
  session came from each source, and how many "pack" assets are past the
  distinctness threshold.

Converted WC3 maps (terrain, placements) arrive the same way: as a loose
folder layered above the MPQ chain. The W client needs no patch archive for
them.

## The pack manifest (format 1, decided 2026-11-04)

A pack is a folder of files and a `manifest.lua` beside them: a Lua
table of plain data, one entry per file, each entry the file's
provenance record (below) with three more fields: `replaces` (the
logical path it answers), `file` (where it is in the folder) and `kind`
(model, texture, animation, particle, sound, font, interface). Lua,
because world-edit-to-execute's manifests are already `manifest.lua`
tables it loads with `loadfile` (its patch layers), and the owner
prefers Lua; plain data only, so the client reads it with a small
reader of its own (`src/059-loose-folders.c`) and runs no Lua for it.
The full layout, field by field, is in `059-loose-folders.h` (and its
`.info.md`). In short:

```lua
return {
    format = 1,
    name = "Wolves, redrawn",
    entries = {
        { replaces = "Creature\\Wolf\\Wolf.m2", file = "wolf/wolf.m2", kind = "model",
          id = "<SHA-256 of the file, 64 hex digits>",
          route = "generate", lineage = "independent",
          generation = { prompt = "...", seed = "18446744073709551615", workflow = "<hash>", models = { "..." } },
          scores = { { score = 0.41, date = "2026-10-30", by = "W05a" } },   -- oldest first
          seal = false,                       -- or { build = 12340, date = "2026-11-01" }
          transcript = "llm-transcripts/nov-01-26.md:120-180" },
    },
}
```

- A field this format doesn't have is refused (a misspelling is caught
  at the start, by name); a later format gets a new number. One table
  is set aside for world-edit-to-execute's own override fields (its
  issue 504: scale, tint, team colour, every map or one): `wc3`, which
  the client doesn't read.
- `licence` is required for `route = "search"`, `generation` for
  `route = "generate"`.
- The seed is a string of digits: a 64-bit number doesn't fit the
  53 bits LuaJIT's numbers hold exactly.
- The seal is `false` or the stock client's build and the day it was
  checked (world-edit-to-execute W05c).
- Only files an entry names are served; a pack file with no entry has
  no provenance, and is named once at the start.

## Similarity score

Every replacement carries a score between 0.0 (identical to the Blizzard
original) and 1.0 (unrelated), with a history, because art is reworked bit by
bit. A replacement counts as **replaced** only past the distinctness
threshold, which is a setting (`assetsDistinctness`: a score at or above
it counts; 0.5 until the owner picks the number, world-edit-to-execute's
open question 10).

How it is measured depends on the kind of asset. Scoring code lives in
world-edit-to-execute (issue W05a); this client reads and displays the scores.

| Asset kind | Measured by |
|------------|-------------|
| Model (mesh + skin) | outline overlap from a fixed camera ring, surface-point distance, colour histograms, image-embedding distance |
| Texture | image-embedding distance, structural similarity, colour histograms |
| Animation | joint-angle curves compared after time alignment (dynamic time warping), per bone, over the whole clip |
| Particle effect | rendered frame sequences compared like W04's recordings (structure + embedding) |
| Sound | spectral and audio-embedding distance |
| Font / interface art | glyph or image outline overlap plus embedding distance |
| Data table rows (spells, factions, display info …) | not scored by similarity; counted as borrowed or ours, row by row. The goal is that no Blizzard row is needed ([106](../issues/106-dbc-parser.md)) |

Models come first; the other kinds follow the same pattern.

## "Default client compatible" seal

A replacement earns the seal when it is:

1. written in the stock 3.3.5a formats (M2 version 264 + `.skin`, BLP2, the
   matching DBC rows),
2. within the stock client's limits (texture sizes power of two, bone and
   texture-unit counts the stock client accepts), and
3. confirmed by actually loading it in the stock client (the recording rig,
   world-edit-to-execute W04).

The seal is a badge on the asset card. Most assets are not expected to have it.

## Provenance record

Every replacement stores how it was made:

| Field | Type | Meaning |
|-------|------|---------|
| id | string (SHA-256) | content hash of the asset files |
| replaces | string | logical path of the Blizzard original |
| route | string | `search`, `generate`, `hand`, `restyle` |
| lineage | string | `derived` (made by looking at or starting from the original) or `independent` (made from text, concept art or reference that never included the original) |
| licence | string | for searched assets: SPDX-style licence, author, URL |
| generation | table | for generated assets: prompt, seed (uint64), workflow hash, model names |
| scores | list | similarity score records over time |
| seal | bool | default client compatible |
| transcript | string | path and line range in `llm-transcripts/` where the work was done |

### Why "lineage" is recorded separately from the score

The owner wants the transcripts to serve as proof of how the game was rebuilt.
They will show exactly that: which assets were made by starting from a
Blizzard original (image-to-3D from a render, a restyle, weights copied from a
Blizzard skeleton) and which were made without looking at one.

These are two different claims. Clean-room reverse engineering keeps the
people who study the original separate from the people who make the new
thing, so the new thing can be shown to be independent. A high similarity
score shows *distance* from the original. It does not show *independence*,
and a derived asset may still count legally as a derivative work however far
it has moved. Recording lineage beside the score lets the project see which
assets rest on which kind of claim, and prefer independent routes where it
matters. This is an engineering record, not legal advice.

## Clean-room by default (decided 2026-09-23)

The owner, answering whether some asset kinds should be made clean-room:
"that may be worth it in all cases. I worry about things like scaling, so it
might need to be an iterative process before a viable model is produced. So
we'll need tests like 'is approximately as large' and if not, then it scales it
up, takes pictures, presents those to the creation system and says 'fix the
imbalances with the new scale' or similar."

So the default route for every asset kind is the **clean-room loop**
(world-edit-to-execute W05d): a *describer* who sees the original writes a
specification in words and measurements; a *builder* who never sees the
original makes the replacement from that specification; a *checker* who sees
both returns only measurements. Assets made that way record lineage
`independent`. The `derived` routes (image-to-3D from a render of the
original, restyles, borrowed skeletons) stay available but are no longer the
default.

## Userspace tools and the shared criteria set (decided 2026-09-23)

The replacement tools run on each user's side; they are not part of the
client. Anyone can make their own art. Players stay in one game because
gameplay depends only on a shared **criteria set** per unit type (body plan,
height and footprint ranges, attachment heights, animation key timings) and on
the map file, never on the art a given player has installed. So this client
must take selection circles, collision, missile heights and hit timing from
the criteria and the map, never from mesh bounds or animation lengths.
Details: world-edit-to-execute `issues/W05d-clean-room-describe-build-check-loop.md`
and `W05e-body-structure-fit-and-body-plan-standards.md`.

## Everything, by generators (decided 2026-10-09)

The owner, 2026-10-09: "we will replace everything we can, bit-by-bit, by
adding 'generators' to each of the potential components. The ultimate goal
is to have one for everything." So the scope is not only what the client
ships: the server's data (quests, NPC text, item names, spell tables in the
AzerothCore database) is in it too. Each kind of component gets its own
generator, a program that makes that kind of thing; they arrive one at a
time, and a component with no generator yet stays borrowed and counted as
such.

**What a player sees** (the owner, the same day, asked what someone who
downloads the finished client and points it at a stock AzerothCore server
should see): "their own models and other assets that they chose to use to
replace one of the assets required by the server. If there are new assets
on the server, they will use those instead of a blank, empty, or broken
model." So the resolver above gains a place to ask: assets the server
itself offers for things that exist only on that server, taken before a
placeholder is ever drawn. A blank or broken model is never the answer
while any source can give a real one.

## What gets replaced, in order

1. Models and their textures (world-edit-to-execute W05, W06)
2. Animation sets (today borrowed from Blizzard skeletons; W05b)
3. Particle and ribbon effects
4. Sounds and music
5. Fonts and interface art
6. Terrain textures and doodads used by converted WC3 maps
7. The server's data: quests, NPC text, item names, spell tables (by
   generators, 2026-10-09)

## Open questions

1. ~~Server-side data in scope?~~ Answered 2026-10-09: yes, everything,
   bit by bit, by a generator per component ("Everything, by generators",
   above).
2. ~~Clean-room split for some asset kinds?~~ Answered 2026-09-23: for all
   kinds, by default (section above).
3. ~~**Where the server's new assets come from.**~~ Answered 2026-10-09,
   the owner: "the server can deliver them. Only a custom client can
   accept them. A player's own replacements are higher priority than the
   server's." So the server sends them; a stock client never asks for or
   takes them; and in the lookup they come after the player's packs
   (above).
4. ~~**The server's assets against Blizzard's.**~~ Answered 2026-10-09,
   the owner: "priority is: player chosen, server supplied, blizzard
   original" (the lookup's order above, now decided), and they travel
   beside the game connection, "using our asset browser, which runs on
   rmail". The asset browser is world-edit-to-execute's planned phase 6B
   (browse, import, preview, edit and share the assets of a map or a
   server; its `issues/908-import-manager.md`), and its rmail road is that
   project's `issues/609-shared-map-and-model-list.md`: a list goes first,
   only the missing files follow as rmail attachments, and rmail's consent
   form is the prompt (the owner, 2026-09-23: rmail is "the only
   connection protocol for assets that I trust").
5. ~~**Joining a server, and what you see.**~~ Answered 2026-10-09: no
   separate acceptance. The owner: "the user who did not choose their own
   wolf model will use the server supplied wolf model. It can download when
   we see it for the first time, and until it's loaded properly we can use
   the Blizzard default version. But only if the user extracts the models
   from their own client, which will be a requirement until we develop our
   own baseline standard model set." So 609's "joining never changes what
   you see" holds for what the player chose (their own models always win)
   and not for what they left to the defaults. Two consequences:
   - **A requirement, for now:** the player supplies the original game's
     models from their own copy of the game, until the project has its
     own baseline standard model set. That set is then what stands in
     while a server's asset is on its way.
   - **rmail's consent form** (every attachment waits for the receiver's
     yes) has to be answered for the player when the sender is the server
     they connected to: 609's "remembered choices" hook, which answers
     consent for trusted senders, is that road.
6. ~~**When a server's assets arrive.**~~ Answered 2026-10-09: before
   logging in, "Let's re-use the editor project's machinery if we can"
   (the owner). The server's list is compared with what the player holds
   and everything missing is fetched while the login screen waits, with
   world-edit-to-execute's pieces: its `issues/609-shared-map-and-model-list.md`
   (the list first, then only the missing files, over rmail), `604`
   (every file stored once, by the hash of its bytes, so nothing held is
   fetched again) and `605` (the local store those land in). The client
   uses them rather than a second copy. An
   asset a server adds while a player is on reaches them at their next
   login. The two shapes as they were weighed (the second "might be
   cheaper / easier", the owner):
   - **On first sight:** an asset is fetched when it is first needed; the
     original game's version is drawn until it has arrived and loaded, then
     swapped in. Nothing is fetched that the player never meets, but the
     world changes under the player as things arrive, every asset needs a
     moment where it is swapped, and a slow delivery shows the old version
     for a while.
   - **Before logging in:** the server's list is compared with what the
     player already holds, and everything missing is fetched before the
     world opens. One wait, at a known moment; nothing swaps in play; the
     lookup only ever sees files on disk. It fetches assets the player
     may never meet, and a server that adds assets while players are on
     needs them fetched at their next login (or a way to fetch mid-game).
   Chosen: before logging in.
