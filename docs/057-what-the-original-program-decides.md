# 057: What the Original Program Decides

*Written 2026-10-16 from the owner's question: "what parts of the client are
there that aren't provided as a data-only format? What can't we extract from
the official client as is?" A checklist for the clean-room describer.*

The original 3.3.5a client is two things. Its **data** (the MPQ archives: DBC
tables, M2, WMO, ADT, BLP, sounds, fonts, and the whole default interface as
Lua and XML) this client already reads ([003](./003-asset-formats.md)). Its
**program** (`Wow.exe`) holds the rules for what to do with that data. The
data files are the nouns, and the program holds the verbs. A spell's table row
names a precast visual, a missile and an impact. Only the program knows the
precast loops until the cast bar fills, the missile flies in an arc, and the
impact lands at the target's chest, not its feet.

This page lists those verbs as **values and behaviors to be filled in**, in
the form: *to behave like the original, this must be that.* Every answer cell
starts empty. Nothing here is the original's code, and nothing written into
it may be.

## How this page stays legally distinct

The plan behind it, and why each rule exists. This is an engineering record,
not legal advice. For certainty, ask a lawyer.

1. **Values and behaviors, never code.** A gravity constant, a heartbeat
   interval, "event A fires before event B" are facts about how a program
   behaves. Copyright protects the way code is written, not the facts it
   carries out (the idea/expression split). So answers here are numbers,
   orderings and plain sentences. No code fragments, no pseudocode that
   follows the original's structure line by line, no names of the original's
   internal functions, no addresses inside the original program.
2. **Public sources first.** AzerothCore's source (the server has to agree
   with the client, so it already carries gravity, speeds, reach and the
   update-field layout), wowdev.wiki, and the original's own interface files.
3. **The decompilation describer second.** Decided by the owner on
   2026-10-16: "let's just build a list, pass it to the decompilation
   describer, and then wait and see what happens." The decompilation the
   owner found (SatyPardus/wowdecomp) has no licence, and the original's
   licence agreement forbids taking the program apart. That is why only the
   **describer** reads it, and writes each answer here in their own words as
   a value or a behavior (see "Brief for the describer" below). The
   **builders**, every session that writes C for `src/`, never see the
   decompilation, only this page. This is the same split as the asset
   clean-room loop ([012](./012-asset-replacement-and-provenance.md),
   "Clean-room by default").
4. **Watching the running client: rarely, and only by machine.** Measuring
   the original while it runs (timing a jump, capturing packets,
   photographing a torch at night) is the cleanest source, but done by hand
   it puts the owner in the loop for every row. The owner, 2026-10-16: "it
   requires too much manual intervention ... trying to minimize the human in
   the loop to only making decisions." So it is used only where a machine
   can do the watching, or where a row is a picture the owner is already
   checking in [046](./046-ingame-checks.md).
5. **Names the original publishes are fine.** Interface function names, event
   names and setting names ("CVars") appear in the original's shipped
   interface files and are the interface addons talk to. Using the same names
   is what lets addons run unchanged, and an interface is not the code behind
   it.
6. **Each answer records where it came from:** `watched` (measured in the
   running client), `server` (AzerothCore), `wiki` (wowdev.wiki or similar),
   `data` (the original's own data files), or `described` (a describer's own
   words after reading the decompilation). The transcripts and this column
   together show how each fact was found.

## How to fill a row

Write the value in the **Answer** column, the source in **From** (one of the
five words above), and the date. Where an answer needs more than a cell,
write it under that section's table as a numbered note on this page. Long
answers belong in the docs, never in issue files. The owner, 2026-10-16:
"in the docs. Not in the issue files, those are our own blueprints. The
blueprints can reference the document though." When the C is written, the
issue that built it links to the row or note here.

## The original's quirks: copied, with a correction beside them

When the original behaves oddly (an interface function returns a value one
frame stale, an event fires twice, a number is rounded the wrong way), the
answer records the odd behavior exactly, and the row is marked
**quirk**. The owner, 2026-10-16: "original first, but write a patch for it
at the same time that we re-implement the behavior. That way, we can just
disable that patch if it turns out to be load bearing to something."

So a quirk is built as two pieces at once:

- **The original's behavior**, written as the spec says.
- **A correction**, a separate switch that fixes it, written at the same
  time. The owner, asked which way the switch starts: "we want to
  reimplement the odd behavior. Then, we should write a patch to fix it at
  the same time. That way, we can turn it on or off as desired." So each
  correction is a setting the player can turn on or off. It starts **off**
  (the original's behavior), following "original first". If an addon turns
  out to depend on the quirk (the quirk is "load bearing"), the correction
  simply stays off. The correction never replaces the original's behavior
  in the code. Both stay, and the switch chooses between them.

A builder adds each correction's switch to the list of corrections in the
issue that builds the behavior, and checks that both choices work (house
rule: when a mode is added, both are tested).

## Brief for the describer

Hand this section to the describer session, with the tables below.

- **You** read the decompilation
  (`/home/ritz/games/azeroth-core/decomp-client/`; its own `CLAUDE.md`
  says the same as this brief). From the moment you read any file there,
  for the rest of your conversation, **this page is the only file you
  change**: never the code, tests, scripts, issues or other docs, and never
  a commit of them.
- For each row you can answer, write: the value or behavior, in your own
  plain words; `described` in **From**; the date. Numbers come with their
  unit (yards, seconds, milliseconds, degrees).
- **Never** copy: code, code-shaped step lists, the original's internal
  function, variable or class names, addresses or offsets inside the
  original program, or the decompilation's comments. Names the original
  ships in its own interface files (interface function names, event names,
  setting names) are fine.
- An ordering is a sentence ("A happens, then B, then C, in the same
  frame"), not a call sequence.
- A behavior that looks like a bug is written down exactly as it is and
  marked **quirk**. Don't fix it in the answer.
- A row you cannot answer, or that turned out to be the wrong question:
  leave the answer empty and write why under the table. A row the tables
  missed: add it under the table as a new numbered note, marked **new row**.
- Answer the rows in the order "Where the describer starts" gives.

---

## 1. The interface runtime

The default interface is data, but it calls about two thousand functions that
exist only inside the program. The largest gap, and the one the owner's rule
"WoW addons must run unchanged" depends on ([006](./006-ui-and-addons.md)).

| To behave like the original… | Answer | From |
|---|---|---|
| each interface function returns these values, in this order, for: a normal case, no target, an out-of-range target, a not-yet-cached unit or item | | |
| each widget method (frame, texture, font string, scroll frame, slider, edit box, model, cooldown, status bar) does this, with these defaults | | |
| the full list of events and each one's arguments, in order | | |
| events in the same frame fire in this order (for example: target changed against the target's health update) | | |
| when a frame's anchors point round in a circle, the original does this | | |
| a frame's effective scale is computed like this from its parents | | |
| frame strata and frame levels sort in this order; a new child's level starts at this value | | |
| text that doesn't fit wraps or truncates by these rules; the ellipsis appears here | | |
| each frame's per-frame update runs in this order against event dispatch; the time-since-last-update value is measured like this | | |
| "taint" spreads by these rules; these functions are protected; combat lockdown starts and ends at these moments | | |
| these actions require a real key press or click | | |
| XML templates inherit by these rules; a virtual frame is handled like this; an XML error does this | | |
| saved variables are written in this text format, at these moments (logout, reload, crash) | | |

## 2. Movement and physics

([801](../issues/801-movement-by-the-servers-rules.md),
[802](../issues/802-collision.md)). Check AzerothCore first for each row.

| To behave like the original… | Answer | From |
|---|---|---|
| gravity is this many yards per second squared | | |
| a jump leaves the ground at this vertical speed | | |
| falling stops speeding up at this terminal speed | | |
| fall time is counted from this moment to this moment, and reported in this packet field | | |
| the steepest walkable slope is this angle; steeper ground slides the player down at this speed | | |
| the player steps up ledges this tall without jumping | | |
| each race's collision body is this shape and size (and comes from this table field) | | |
| the player starts swimming when water is this deep compared with body height, and stops at this depth | | |
| while moving, a heartbeat movement packet goes out every this many milliseconds | | |
| these changes send a movement packet at once (start, stop, turn, jump, strafe, pitch, …) | | |
| on a boat or zeppelin, position is kept relative to the vessel like this; boarding and leaving happen at these moments | | |
| between packets, other units' positions are guessed this far ahead, and corrected like this when wrong | | |
| turning speed (keyboard) is this; backpedal speed is this fraction of run speed | | |
| a knockback flies on this curve | | |

## 3. Camera

([207](../issues/207-camera.md)). The settings menu side is in
[048](./048-stock-options.md).

| To behave like the original… | Answer | From |
|---|---|---|
| the camera's default, closest and farthest follow distances are these | | |
| one wheel click zooms this far; zoom eases at this speed | | |
| the camera can look this far up and this far down | | |
| when the camera hits terrain or a wall it moves in like this, and eases back out at this speed | | |
| the camera swings behind a moving character at this speed, after this delay | | |
| mouse-look turns this many degrees per pixel at the default sensitivity | | |
| these Config.wtf settings change camera behavior, in these ways | | |

## 4. Rendering rules

([302](../issues/302-terrain-renderer.md) to
[310](../issues/310-shadows-and-live-light.md),
[401](../issues/401-m2-reader.md) to
[404](../issues/404-characters-and-equipment.md)). The formats are known; the
meaning of some fields is not. [046](./046-ingame-checks.md) holds many
"does it look the same?" questions in this area.

| To behave like the original… | Answer | From |
|---|---|---|
| an M2 batch's texture-unit and blend-mode combination chooses this drawing recipe (one row per combination) | | |
| fog thickens with distance by this formula, starting and ending at these distances | | |
| the light tables' values blend with distance from each light like this, and with time of day like this | | |
| the sky dome has these bands at these heights; stars, sun and moon are placed like this | | |
| terrain alpha layers combine like this; the baked terrain shadow darkens by this much | | |
| water ripples run at this rate; its surface colour and see-through amount come from these values | | |
| small objects stop being drawn at these distances; models swap detail levels at these distances | | |
| particles and ribbons update each frame by this formula (check wowdev.wiki first) | | |

## 5. Characters and animation

([402](../issues/402-skeletons-and-animation.md),
[404](../issues/404-characters-and-equipment.md))

| To behave like the original… | Answer | From |
|---|---|---|
| each piece of a character's skin texture (upper arm, lower arm, hand, upper torso, lower torso, upper leg, lower leg, foot, face upper, face lower) is pasted at this pixel rectangle | | |
| an item's display info shows and hides these mesh pieces; when two items claim the same body area, this one wins | | |
| the animation played for: walk, run, backpedal, strafe, combat stance, jump start / loop / land, swim, mounted, each emote | | |
| a model without the wanted animation falls back through this chain | | |
| one animation blends into the next over this many milliseconds | | |
| weapons sit at these attachment points, in hand and sheathed; sheathing happens at these moments | | |
| a rider sits on a mount or vehicle seat like this | | |

## 6. Spell visuals and feedback

| To behave like the original… | Answer | From |
|---|---|---|
| a spell visual's stages (precast, cast, channel, missile, impact, lingering state) start and stop at these moments | | |
| a missile flies at this speed (from the spell table) on this arc shape, aimed at this point on the target | | |
| cast sounds play at these moments | | |
| floating combat text appears at these moments, moves like this, for this long | | |

## 7. Text the client builds itself

| To behave like the original… | Answer | From |
|---|---|---|
| spell description tokens (`$s1`, `$d`, `$o1`, `$/10;s1`, `$l…:…;`, `$g…:…;`, and the rest) mean these things and format like this | | |
| chat links (`\|H…\|h`), colour codes and in-text icons follow this syntax | | |
| numbers, durations and money format like this in each language | | |

## 8. Choices made on the player's side

| To behave like the original… | Answer | From |
|---|---|---|
| tab targeting considers units in this cone, within this distance, sorted like this, and starts over when this happens | | |
| the unit under the mouse is chosen by this test (model shape or bounding box), with this priority when two overlap | | |
| action buttons turn red for out of range by this local check | | |
| a unit's name and circle colour comes from faction, hostility and tapped status like this | | |
| click-to-move, auto-run, interact distance and auto-loot behave like this | | |

## 9. Sound

([007](./007-audio-system.md))

| To behave like the original… | Answer | From |
|---|---|---|
| a 3D sound gets quieter with distance by this formula | | |
| at most this many sounds play at once; when full, this one waits or stops | | |
| zone music and ambience change when this happens, fading over this long | | |
| footstep sounds play on this animation event, chosen by ground type like this | | |

## 10. Settings, caches and startup

| To behave like the original… | Answer | From |
|---|---|---|
| every setting the menu doesn't show (the menu's are in [048](./048-stock-options.md)): its name, default, and what it changes | | |
| the cache files in the WDB folder (creature, item, quest, …) are laid out like this, and refreshed when this happens | | |
| at login and on entering the world, these things load before the loading screen ends; its progress bar measures this | | |
| on disconnect the client does this; a failed login shows this | | |

## 11. The order of one frame

([110e](../issues/110e-per-frame-work-as-stations.md),
[110g](../issues/110g-the-clock.md))

| To behave like the original… | Answer | From |
|---|---|---|
| one frame runs network, objects, movement, animation, interface and drawing in this order | | |
| a packet that arrives mid-frame takes effect in this frame or the next | | |

---

## Where the describer starts

Most valuable first:

1. **The interface runtime** (section 1): the most surface, no public
   description of its behavior, and addons depend on it.
2. **Movement and frame order** (sections 2 and 11): wrong values here make
   the game feel off even when every picture is right.
3. **Character compositing and mesh-piece rules** (section 5): small exact
   facts that cannot be guessed.
4. **Spell text tokens and spell visual stages** (sections 6 and 7).
5. Everything else, row by row, after AzerothCore and wowdev.wiki are checked.

## Open questions

1. ~~**Quirks: copy or correct?**~~ Answered 2026-10-16: both. The
   original's behavior is built, with a correction beside it that the
   player can turn on or off ("as desired"). Each starts off, by "original
   first" (see "The original's quirks").
2. ~~**Where a long answer lives.**~~ Answered 2026-10-16: in the docs
   (here), linked from the issues.
3. ~~**Keeping the builders clean.**~~ Answered 2026-10-16.
   - **The rule:** "If you read anything from the decompilation, you should
     only be touching the checklist." A conversation that has read any file
     of the decompilation changes only this page for the rest of its life.
   - **Where the copy lives:** `/home/ritz/games/azeroth-core/decomp-client/`
     (the same disk folder as `/mnt/mtwo/games/azeroth-core/decomp-client/`),
     beside this project.
   - **How builders are kept out:** a written warning, its own `CLAUDE.md`,
     at the top of the copy, which a Claude session loads when it reads
     anything there. No machine block. The owner: "written warning in the
     location that they HAVE to read. If the machine is too stupid not to
     listen to that warning, then it is absolved of responsibility. This
     means I wouldn't choose a model incapable of heeding that sort of
     directions, and I wouldn't use a model that couldn't tell me no. This
     is my responsibility."
4. ~~**Is the decompilation read at all?**~~ Answered 2026-10-16: yes, by
   the describer, from this list ("build a list, pass it to the
   decompilation describer, and then wait and see what happens").
