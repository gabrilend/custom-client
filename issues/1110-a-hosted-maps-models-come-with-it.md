# 1110 — A Hosted Map's Models Come With It

*Decided 2026-11-04. Came from: world-edit-to-execute issue 609 (the shared
map-and-model list) and its pull request #227 in gabrilend/ai-stuff ("the
game asks about a shared list, and takes it off after"), which the owner
placed here that day ("227 sounds like it belongs to the client, to
me"): using a list during a game belongs to this client. Its decisions
are carried here, and the pull request is to be closed.*

## Current behavior

This client has no WC3 map mode yet and no way to receive another
player's models. The only built version of this lived in
world-edit-to-execute's map viewer (#227, never merged): it checked the
player's rmail inbox every 5 seconds for lists and asked about each one
over the game.

## Intended behavior

The owner (2026-11-04): "we only send models and such when the map that
includes them is shared. This happens either intentionally 'check out my
map' or when the map is hosted 'come check out my map' (see the
difference?) so we don't need to check the rmail inbox every 5 seconds."
And: "227 sounds like it belongs to the client, to me."

Two occasions, two places:

- **"Check out my map"**: one person gives another a map to keep. The
  map, and the list of models the sender uses with it, arrive together
  over rmail, outside any game; world-edit-to-execute's editor and
  launcher receive them (its 609). Nothing here.
- **"Come check out my map"**: a player hosts a map and others join.
  **This issue.** The host's list arrives as part of joining, so there is
  no inbox to watch: the question is asked once, while joining, before
  the map starts.

What joining does:

1. The host's list (the map, and for each model the host replaced: what
   it replaces, its content hash, its size, where it came from) arrives
   with the join. The list is plain data; it is read, never run.
2. **The question**, one dialog, drawn like the map's own dialogs: who is
   hosting, and where each model came from (licence, made how, how far
   from the original), at most 14 lines plus "and N more". Buttons: Yes,
   No, "Always from this player", "Always for this map". Only a mouse
   click answers it; Enter and Escape never do, so a stray key never says
   yes or no. The two "always" answers also mean yes, and are remembered.
3. On yes, only the files this player lacks are fetched (files are kept
   once, by the hash of their bytes), and every file's hash is checked.
   One bad or missing file refuses the whole list: half a list is a mix
   nobody chose.
4. The list becomes a layer in front of the player's own packs in the
   asset lookup (105a) for this game only. **The ready notice**, once:
   "<host>'s models are in use", with "Keep them after the game".
5. When the game ends the layer is taken off, or kept as one of the
   player's own packs, named "<host>'s models for <map>" (a number added
   if the name is taken). A kept choice is written down at once, so a
   crash still keeps it; a game that ended without saying so is settled
   at the next start.
6. Gameplay never depends on it: a player who says no plays the same
   game with their own models (the map's own unit data holds collision,
   heights and timing).
7. Failures are notices, never a stopped game.

Offering a list is the host's side of the same thing: hosting a map
offers the list of models the host uses with it. (#227 had filed this
separately, as "offer a list from the game".)

## Suggested implementation steps

1. The list in the join: carried beside the map in the hosting
   handshake, read by the plain-data reader.
2. The question and the ready notice, drawn by the interface runtime
   (Phase 7) in the map's dialog style.
3. Fetching the missing files and checking hashes (world-edit-to-execute's
   604 and 605: one copy per file, by hash).
4. The one-game layer in 105a's lookup; taking it off or keeping it at the
   end; settling leftovers at start.
5. The "always" answers, saved (707).

## Acceptance

- Joining a hosted map with a list asks once; yes shows the host's models
  for that game only, and no changes nothing.
- A tampered file refuses the whole list.
- "Keep" leaves the models as the player's own pack; otherwise they are
  gone after the game.
- No inbox is checked on a timer.

## Open questions

1. ~~**What carries the list when joining?**~~ Answered 2026-11-04, the
   owner: "the server's own join handshake." So the list rides in the
   messages the server and this client exchange while a player joins,
   which means AzerothCore's side of joining carries it too (a server
   change, by module or patch). The files the player lacks then follow
   the road docs/012 (answer 4) set for a server's assets, beside the
   game connection. The question as asked: the hosting handshake itself
   (through AzerothCore), or an rmail message the host sends to each
   joiner as they join?

## Related

- Blocked by: [105a](./105a-loose-overlays-and-asset-resolver.md) (the
  lookup), Phase 7 (dialogs), [707](./707-saved-variables.md), WC3 map
  mode (1101-1107).
- world-edit-to-execute issue 609 (the list format, the "check out my
  map" side), its `docs/wow-client-bridge.md`.
- [docs/012](../docs/012-asset-replacement-and-provenance.md) (the
  server's assets and the player's own).
