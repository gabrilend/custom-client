# 012 — Asset Replacement and Provenance

How the W client moves from Blizzard's assets to its own, and how it keeps a
record of how each replacement was made.

## The resolver

Every asset request (model, texture, animation set, particle definition,
sound, font, interface image) goes through one function that asks three
places in order:

```
 logical path ("Creature\Wolf\Wolf.m2")
      │
      ├─ 1. replacement packs (loose folders, ours)          → source = "pack"
      ├─ 2. the owner's Blizzard client (MPQ chain)          → source = "blizzard"
      └─ 3. placeholder (magenta checker / wireframe / beep) → source = "placeholder"
```

- A "blizzard" answer is allowed and counted. It is the normal state today.
- A "placeholder" answer is a warning; strict mode makes it an error.
- The HUD and every phase demo show the counts: how many assets drawn this
  session came from each source, and how many "pack" assets are past the
  distinctness threshold.

Converted WC3 maps (terrain, placements) arrive the same way: as a loose
folder layered above the MPQ chain. The W client needs no patch archive for
them.

## Similarity score

Every replacement carries a score between 0.0 (identical to the Blizzard
original) and 1.0 (unrelated), with a history, because art is reworked bit by
bit. A replacement counts as **replaced** only past the distinctness
threshold, which is a setting.

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

## What gets replaced, in order

1. Models and their textures (world-edit-to-execute W05, W06)
2. Animation sets (today borrowed from Blizzard skeletons; W05b)
3. Particle and ribbon effects
4. Sounds and music
5. Fonts and interface art
6. Terrain textures and doodads used by converted WC3 maps

## Open questions

1. Server-side data (quests, NPC text, spell tables in the AzerothCore
   database) is also Blizzard-derived. Is it in scope for "legally distinct",
   or is the goal the client-side game only?
2. ~~Clean-room split for some asset kinds?~~ Answered 2026-09-23: for all
   kinds, by default (section above).
