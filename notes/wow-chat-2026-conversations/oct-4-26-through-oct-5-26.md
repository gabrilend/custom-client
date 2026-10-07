# Conversation Summary: 3cca55b1-1e96-4489-a864-41157a21299a

Generated on: 2026-10-05 05:03:03
Models: claude-opus-5-5

## Contents

1. 2026-10-04 20:27, after Request 5 - We're getting your basic-profile server
   working and adding features; Growth's seven buddies are now geared and back
   with you, and Sargobras's fire-and-sit behaviour is live. Next, please answer
   whether to rename the buddy gear command from "elite" to something like
   "outfit".
2. 2026-10-04 21:12, after Request 9 - Goal: build and tune the buddy bots for
   your basic-profile server. The tank-and-healer draw, the aggro trigger and
   the 3-second fix are committed but waiting on your compile. Next: compile,
   restart, and answer the compass-formation questions.
3. 2026-10-05 00:03, after Request 12 - Building out Everland's buddy-bots and
   basic-profile tuning; the latest spirit-healing patch and death knight story
   skip are committed and waiting on your compile. Next, please answer whether
   casters should also use the new stamina-based spirit healing.
4. 2026-10-05 03:04, after Request 22 - We're planning basic's dungeons, raids
   and buddy behaviour, and just recorded your wiki, campfire, scroll and pacing
   answers in the issues. Next: your call on how the wiki ranks changes, and the
   starting pacing setting.

--------------------------------------------------------------------------------

### User Request 1

hi um I tried to run the authserver/worldserver scripts and they didn't properly
start up mysql. They're supposed to. Can you look into it? And then when you're
done, can you do an audit for cleanup tasks we could do? There's an unsorted
issue or two in the project directory, and I'm sure we could clean out some
cruft, maybe do a little refactoring...

--------------------------------------------------------------------------------

### Assistant Response 1

> Checking why MySQL didn't come up — the PID file from yesterday is still
> there, which hints the scripts thought MySQL was already running. Testing that
> theory now.

> `stop-mysql` is patched. Next: lengthening its shutdown wait, then pointing
> both launchers at the new `start-mysql`.

> Now the launchers: replacing each copy of the old check with a plain call to
> `start-mysql`, which now does the checking itself.

MySQL is running now and the test passes. Loading the project's commit workflow
before committing.

--------------------------------------------------------------------------------

### User Request 2

Base directory for this skill: /home/ritz/.claude/skills/issue-lifecycle

# Issue lifecycle

The owner's CLAUDE.md is the authority on what issue files are for and what
they must contain; where it and this skill ever disagree, CLAUDE.md wins. This
skill adds the mechanics CLAUDE.md leaves open: which tools answer which
question, in what order, and what to do when a tool says no.

All tools live in one place and are called by absolute path:

| Tool | Answers |
|---|---|
| `/home/ritz/programming/ai-stuff/scripts/validate-issues <project>` | is the issue tree consistent; next free number per phase |
| `validate-issues <project> --next <phase>` | the number to give a new issue |
| `validate-issues <project> --file <issue>` | is this one issue well-formed and are its links matched |
| `/home/ritz/programming/ai-stuff/scripts/progress-dashboard.lua <project> -m` | done/open counts per phase (for progress files and demos) |
| `/home/ritz/programming/ai-stuff/scripts/commit-own-changes <repo> -F -` | commits exactly the lines this session wrote, plus the changed transcripts of the commit's projects (any conversation's), and runs the repository's commit hooks, without touching the shared staging area |
| `/home/ritz/programming/ai-stuff/scripts/stage-own-changes <repo>` | previews what that commit would take; writes nothing |
| `/home/ritz/programming/ai-stuff/scripts/claim-own-change <file>` | records lines this session changed by a route the edit ledger cannot see (see Committing) |
| `/home/ritz/programming/ai-stuff/scripts/adopt-left-behind-changes <repo> -- <path>...` | takes ownership, line by line, of uncommitted work a finished session left, leaving lines any other session still holds (see Committing) |

Each tool has a `.info.md` beside it; read that rather than the source.

## Before writing a new issue

1. **Search first, including `issues/completed/`.** Grep the issue tree for
   the feature's nouns, not just the likely title. A completed issue about the
   same machinery is reopened and extended rather than duplicated: history is
   more useful stacked vertically in one file than spread across several.
2. **Pick the phase by what the work builds on**, not by when it is being
   done. Lower numbers are foundations; later issues depend on earlier ones.
3. **Take the number from the tool:** `validate-issues <project> --next
   <phase>`.
   It reads the project's own naming shape (522, 1001, 9-007, A04) and never
   reuses a number that a completed file already holds.
4. **Write the blueprint** with the three sections CLAUDE.md requires, plus
   link fields in the shape the project already uses (look at a neighbour).
   Name related functions, structures and files instead of pasting code.
5. **Check it:** `validate-issues <project> --file <new-issue>`, and fix what it
   reports on the new file. If the new issue blocks or is blocked by others,
   add the matching line to the other side too.

## While working

- Keep **Current Behavior** true. It is the only section that changes while
  an issue is in progress; rewrite it in place rather than appending a log.
- A question that surfaces is written into the issue (an "Open questions"
  section) and then asked. An issue with an unanswered question is in
  progress, not done.
- Anything discovered that changes another issue's assumptions is written into
  that issue now, not at completion.

## Splitting into sub-issues

Split when an issue holds work streams that could be built, tested or reviewed
separately, or when it cannot be finished in one sitting. Do not split an issue
that is already one mechanism. When splitting, produce and then write out:

1. **A table** — `| ID | Name | Dependencies | Description |`, where ID is the
   parent number plus a lower-case letter (103a, 103b), Name is dash-separated
   lower-case words, Dependencies is "None" or sibling IDs.
2. **The rationale** — the distinct work streams, why one issue cannot hold
   them, what splitting buys.
3. **The execution order** — a small dependency graph, e.g.
   `103a (foundation) → 103b (needs 103a) → 103c (parallel with 103b)`.

Each row becomes a file `{parent}{letter}-{name}.md` with the three sections.
The parent keeps its blueprint and gains a short list of its sub-issues; the
analysis itself is not appended to the parent (that would be worklog, not
blueprint). Run `validate-issues` afterwards: it reports orphaned sub-issues
and one-sided links.

## Completing an issue

An issue is complete only when nothing in it is deferred and no open question
is unanswered. Then, in this order:

1. **Run the project's tests.** A fixed bug gets a test that would have caught
   it.
2. **Rewrite the issue as the blueprint of what was built**: Current Behavior
   states the built system; steps name the real functions and files; decisions
   not taken are stated with their reason. Poetry the owner wrote during the
   work goes into the issue verbatim.
3. **Move it** into `issues/completed/` (`git mv`, so the history follows).
4. **Update `issues/phase-<N>-progress.md`** for that phase. Take counts from
   `progress-dashboard.lua <project> -m`, or better, name the command instead
   of copying numbers that will go stale.
5. **Update related issues** whose Current Behavior or assumptions changed.
6. **Run `validate-issues <project>`** and fix new findings that the change
   caused. Findings older than the change are reported to the owner, not
   silently fixed in passing.
7. **Commit** as below.

## Committing

The rule (CLAUDE.md): a commit carries exactly the lines this session wrote —
never another session's work, even if it sits in the same file — and every
changed transcript of the commit's projects rides along — the session's own
project folder and the projects of the files it commits — this
conversation's and any other's left behind (a session that committed, talked
on and quit), so each project's story in git stays whole.

Commit small and often: each piece as soon as it is done and checked, not a
pile at the end.

1. Optionally preview: `stage-own-changes <repo>` lists what would be taken
   and what left out, and writes nothing.
2. Commit: `commit-own-changes <repo> -F -` with the message on standard
   input. It builds the commit on a private staging list from this session's
   own lines (the edit ledger the edit hook keeps) plus those transcripts, and
   never touches the shared staging area, so nothing another
   session has staged can ride along. Plain `git commit` is refused by the
   commit gate.
3. Message style: say what the software now does, in plain English and by
   mechanism or analogy; mention any extra changes and why. No function names.

**When it stops or leaves something out.**

- **A tangled ("mixed") block** means another session changed lines right
  next to or on top of yours. Nothing is committed. Tell the owner which file
  and line; decide together what that region should say. `--leave-mixed`
  commits your other blocks meanwhile.
- **Lines that are yours but were written by a route the ledger could not see**
  (a script you ran, a generator, `sed`, a rename): record them with
  `claim-own-change <file>` and say so in the commit message.
- **Nothing to commit, but the work is there**: an earlier session wrote it
  and ended, or the machine rebooted and emptied the ledgers. Look first with
  `adopt-left-behind-changes <repo> --dry-run -- <path>...`, then adopt and
  commit one batch at a time with `commit-own-changes <repo> -F - --
  <path>...`. Lines reported as **held** belong to another session. Take them
  (`--from <session>`) only if that session has finished, and ask the owner
  if unsure. Say in the commit message that the work was adopted and from
  whom. This is the route for a left-behind backlog, not the token.
- **A warning about the shared staging area** means someone had staged that
  file by hand in a way that overlaps your commit; their entry was left alone.
  Tell the owner.

The one-time permission tokens (`/tmp/claude-allow-*`) belong to the person.
Never create one; if a gate names a token, relay the gate's message and let
the owner decide.

## Open questions

- Phase ≥ 10 in compact names depends on the project's issue-number width;
  `validate-issues` decides it from evidence (progress files, `phase-N-demo`
  names). A project with none of those gets a warning. Should the house rule
  fix one width (two digits, as in the 522 example) for every new project?


ARGUMENTS: commit only this session's fix (start-mysql, stop-mysql, authserver,
worldserver, new test-mysql-start) with transcripts riding along; bugfix, no
issue file

--------------------------------------------------------------------------------

### Assistant Response 2

> Previewing the commit and recording the edits I made with scripts, which the
> change tracker can't see.

*[commit] b21cb91c9 in wow-chat-2026 - The game servers start MySQL themselves again, even after a reboot*

> The MySQL fix is committed. Starting the cleanup audit; this part only reads
> files and changes nothing.

> Still auditing. I'm checking how much code the scripts copy between each
> other, which tells us where refactoring would pay off most.

## MySQL startup: fixed, tested, committed

The authserver and worldserver logs from 17:33 both say "Can't connect to MySQL
on port 3307". MySQL was never started at all.

**Cause:** when MySQL runs, it writes its process number (3838 here) into a file
in its data folder. To decide whether MySQL was running, the launchers only
asked "is any process using number 3838?" After this morning's reboot, the
system had handed 3838 to some unrelated program. The launchers saw a match,
assumed MySQL was up, and skipped starting it.

**What changed:**
- **The starter** now accepts the number only if that process is running this
  project's own MySQL program. A file that fails the check is treated as left
  over and deleted.
- **Waiting for ready:** the starter used to report success as soon as that
  number file appeared. MySQL writes it several seconds before it accepts
  connections, so a server could still lose that race. The starter now waits
  until MySQL answers a ping. It took about 5 seconds on this machine.
- **Failed starts:** if MySQL dies on the way up, the starter says so straight
  away and prints the last lines of MySQL's error log. It also refuses to start
  a second copy on top of a running one. Yesterday's error log shows a second
  copy failing on a locked data file, which is what happens when that occurs.
- **The launchers** now always call the starter, which is safe to run when MySQL
  is already up.
- **The stopper** had the same hole, and there it could have force-killed an
  unrelated program. It now has the same check, and waits 60 seconds instead of
  10 before forcing MySQL down, since a forced stop makes MySQL repair itself on
  the next start.
- **New test, `scripts/test-mysql-start`:** it fills the number file with a
  live, unrelated process's number and checks that MySQL still comes up,
  answers, and isn't started twice. All 8 checks pass.

MySQL is running now, so `./scripts/authserver` and `./scripts/worldserver`
should start for you. The commit also carried the pending transcript files, as
the project's commit setup does.

## Cleanup audit, most valuable first

Nothing below has been changed yet.

1. **A half-finished file rename is still staged.** Last night's commit that
   retired two source patches added the renamed copies, but never committed the
   deletion of the old files. That deletion is still staged.
   - Committing it finishes that rename. Leaving it means the next plain commit
     could pick it up by surprise.
2. **About 6 GB of build folders nothing uses.** Two old build-output folders,
   one from April 9 (3.5 GB) and one from April 13 (2.4 GB), aren't referenced
   by any script. Both are already excluded from git.
   - The alpha profile's source folder (1.9 GB) is still used by many scripts,
     but the alpha install folder is empty, so alpha has apparently never been
     built here. Whether it's worth keeping is your call.
3. **The issue tree has 223 consistency findings across 306 issues.** The issue
   checker found:
   - 69 issue files without a `.md` extension. Renaming them is mechanical.
   - About 120 issues missing one of the three required sections (current
     behavior, intended behavior, suggested steps). These need writing, not just
     reshuffling.
   - 25 places where issue A says it's blocked by issue B, but B doesn't list A
     as something it blocks. These are mechanical.
   - Three numbers used by two files each:
     - The ALE game-object wildcard issue exists twice. The completed-folder copy is the older pre-migration version, and the open-folder copy says "Completed". The open copy belongs in the completed folder.
     - The ALE query dangling-pointer issue and the symmetric aggro radius issue each share a number with an unrelated completed issue. They need new numbers.
   - One issue depends on three issues that don't exist.
4. **Copied code across the scripts.**
   - 18 scripts each read the active profile themselves.
   - 11 scripts each have their own copy of the table that maps a profile to its
     source and install folders.
   - Moving these into one shared file would mean a new profile, like the
     planned "expert", is added in one place instead of eleven. The MySQL check
     I just fixed was copied into two scripts the same way.
5. **The temporary folders don't match the house layout.** The project's tmp
   link points at `/tmp/wow-chat-2`, but the shared-memory link inside it points
   at `/dev/shm/wow-chat-2026`. The house layout uses the project name for both,
   so tmp should point at `/tmp/wow-chat-2026`.
6. **Retired files past their one-commit grace period.**
   - These have been in at least one commit and could be removed: the retired
     old launcher script, the profile-switch script, the config-registry script,
     and a jeweler-spot reader marked as done.
   - Two other files still mention the old launcher and would need checking: the
     credential-manager issue, and the completed issue that split that launcher.
7. **Root-level clutter.**
   - Three PNGs sit in the project root: the connection guide, a progress update
     and "remembrance". They could move to `assets/`.
   - A `credentials.bak` sits in `scripts/`. It's excluded from git, but it may
     hold passwords.
8. **The two unsorted notes:**
   - **"Actually, can I go somewhere else?"** is closest to the rotating faction
     starting valleys issue. That issue already decides which valley each new
     character starts in. Your note adds a per-player "deck" on top: one gossip
     option on Sargobras deals the next same-faction valley without replacement.
   - **"A disembodied mind journey through a problem space"** reads as a vision
     fragment about how bots choose their paths. It could go into `notes/`
     beside the other vision files.

## Open questions

1. Should the starting-valley "deck" be a new sub-issue under the
   rotating-valleys issue, or under the Sargobras buddy-selector issue? It
   touches both.
2. When the deck runs out, should it reshuffle? And does using it count against
   the faction-wide rotation, or bypass it?
3. Can the two old build folders (about 6 GB) be deleted? Is the alpha source
   worth keeping?
4. Which cleanup items should I take on, and in what order? My suggestion is 1,
   then the mechanical parts of 3, then 4.

--------------------------------------------------------------------------------

### User Request 3

<pasted_content id="7f61">
>      - "Actually, can I go somewhere else?" is closest to the rotating faction starting
>        valleys issue. That issue already decides which valley each new character starts in.
>        Your note adds a per-player "deck" on top: one gossip option on Sargobras deals the
>        next same-faction valley without replacement.
</pasted_content id="7f61">


once a character hits level 2, we can drop that option / database record for
them. If they're ever reset to level 1 for any reason, we can re-create it.
Question... How would we do that? An on-login lua hook? Sounds performatively
expensive.



<pasted_content id="7f61">
>      - "A disembodied mind journey through a problem space" reads as a vision fragment
>        about how bots choose their paths. It could go into notes/ beside the other vision
       files.
</pasted_content id="7f61">


it's also related to the LLM personality system.

could we add an issue file for the custom client that packages llama.cpp or
similar into the client? then, we can "shard" or "distribute" llm compute tasks
to all the players who wanted to contribute. We'd also have a UI gizmo that
showed exactly what was being done on their machine. "oh an orc in Stranglethorn
is talking to a Night Elf" very abstracted, nothing specific, but they'd still
be able to see the text that was being generated. It'd show the processing task
with as many little dials and gauges and bar charts and sliders that we can
think of, because players love those kind of details. Plus we could let them
fiddle with the way that characters are processed too - maybe even adding their
own custom system prompts. In this way, each player gets to contribute to the
nature of the collective. The system prompts that players can provide should not
determine behavior (to the best of our ability) but rather the *nature* of the
character who is being performed. Their qualities, and

--- stack overflow ---

oh hey we should change the movement speed of different models depending on
their model size, not some arbitrary number. The current config value (80% speed
I think) is ideal for Blood Elf F

Tauren, being the largest, should move the fastest and farthest.

*plainsstriding*

--------------------------------------------------------------------------------

### Assistant Response 3

> Both servers connect now: the new logs show the auth and world servers opening
> their databases and loading the world.
>
> Next I'm checking for existing issues on the custom client, the LLM
> personality system and movement speed, so the new ideas can extend them
> instead of duplicating them.

> Writing the two issues now: player-contributed LLM compute as a new sub-issue
> of the bot-chat module, and movement speed by body size as a new phase-1
> issue.

I wrote two issue files. Both pass the issue checker, and neither is committed
yet: your rules tie commits to finishing an issue.

## The "somewhere else?" deck: no database row, no login hook

There's a way to avoid storing anything per player, so there's nothing to drop
at level 2 or rebuild at level 1:

- **The deck order:** shuffle the faction's starting valleys once, using the
  character's unique id number as the random seed. The same character always
  gets the same order, so it can be recomputed whenever it's needed.
- **Your place in the deck:** this is the valley you're standing in. Pressing
  "Actually, can I go somewhere else?" sends you to the valley after it in your
  order. That deals every valley exactly once before wrapping around, like a
  deck of cards, with only one button.
- **The level-1 limit:** Sargobras only shows the option when the player is
  level 1. At level 2 it disappears without any cleanup. If a character is ever
  reset to level 1, it reappears, with the same deck order as before.

On the cost question: a login hook would actually be cheap. It fires once per
login, and a login already makes the server load dozens of tables. A
level-change hook is cheaper still. The design above just needs neither. The
only work happens when someone clicks the button.

A question back, because it breaks this design: the starting-valley rotation
also sets the hearthstone home to the valley the character starts in. If the
deck moves a character but their home stays behind, the hearthstone takes them
back to a valley they deliberately left. What should happen? Should the home
move each time they're dealt a new valley, or only once they've decided to stay?

## Player-contributed LLM compute: `issues/916o-player-contributed-llm-compute.md`

It's filed under the bot-chat module, because that module already has the pool
of inference machines and the per-bot personas. Your words are quoted in it
verbatim, with the cut-off marked. The main technical point it records: the
3.3.5a client is a closed program, and its addons are sandboxed Lua with no
network access, so the model can't run inside the game.

The design that follows from that:
- **The worker:** a small separate program (llama.cpp's server) ships with the
  client download. It connects out to the server, so players don't need to open
  any ports, and joins the same rotation as the LAN machines.
- **The gizmo:** an addon window with the dials, gauges, sliders and streaming
  text. The server sends it summaries of each job, along the lines of "an orc in
  Stranglethorn talking to a night elf".
- **Only chat goes to players.** The prompts that decide what bots do never
  leave the server, so a player-written prompt can shape a character's nature
  but not its behavior. Replies still go through the existing check that accepts
  only a single in-character line.

It has six open questions. The first is how "Their qualities, and…" was going
to end. Another asks whether this reopens your earlier note that persona design
isn't where you want to spend effort.

## Movement speed by body size: `issues/166-movement-speed-by-model-size.md`

The client's data files already hold each race's model height, so a generator
can work out the speed factors and nobody types numbers in by hand.

One thing from the server code changes how this can be built. The server
recalculates run speed from scratch whenever any speed effect changes, such as
mounting, sprinting, a slow, or water. So a speed set from Lua would be wiped
the first time the player mounts. The durable options are a hidden passive speed
aura per race, or a small source patch to that recalculation.

It has six open questions; the first is the shape of the curve:
- **Linear:** speed in direct proportion to height. Gnomes end up at roughly
  half a blood elf's speed.
- **Square root:** follows how real animals' walking speed scales with leg
  length. Gnomes end up around 0.57 and tauren gain less.

I'm not sure of the gnome-to-blood-elf height ratio yet; the generator in the
first step will print the real numbers.

While checking this I found the project notes list a module that scales player
models by level ("mod-grownup"), but it isn't in the source tree.

`vision-mindscape` is still in the project root. Since you tied it to the
personality system, it's quoted in the compute issue's origin and its open
questions. Should the file itself move into `notes/`? And the four questions
from the cleanup audit are still open.

--------------------------------------------------------------------------------

### User Request 4

and can we also disable the map and minimap? Sargobras follows too quickly

sargobras isn't walking when I walk, he runs, then stops, then runs... also, he
follows too closely, you should have to walk a bit (just back from where you've
come).

hi can you equip Growth's buddy-bots with gear that is roughly equivalent? They
can be from any previous tier, but nothing beyond Molten Core. No Onyxia or
Zul'gurub either. Just MC and dungeons and previous stuff. No PvP gear either.

> - Your place in the deck: this is the valley you're standing in. Pressing
> "Actually, can I go somewhere else?" sends you to the valley after it in your
> order. That deals every valley exactly once before wrapping around, like a
> deck of cards, with only one button.

ah yes, but it won't be the fastest possible combination of destinations you
could pick. That'll always be the probabilistic one, because there's a chance of
getting it faster and no chance of getting it slow. Only... There's exactly the
same number of outcomes with the same type of outcomes as with one in a row, so
yeah it's pretty random. But that's kinda the point? To make each character shop
around. Not the player, who knows the order, and not the geography, that is
always the same - but the random chance of destimagic.



<pasted_content id="7f61">
> A question back, because it breaks this design: the starting-valley rotation
> also sets the hearthstone home to the valley the character starts in. If the
> deck moves a character but their home stays behind, the hearthstone takes them
> back to a valley they deliberately left. What
> should happen? Should the home move each time they're dealt a new valley, or
> only once they've decided to stay?

</pasted_content id="7f61">


hearthstone must be set correctly

> The client's data files already hold each race's model height, so a generator
> can work out the speed factors and nobody types numbers in by hand.

the height factors won't be accurate because we need to assess their gait, which
relies mostly on their hips and shoulders. However, a crude scaling is alright
for now, until we can implement the full version in a cleanup pass.

> One thing from the server code changes how this can be built. The server
> recalculates run speed from scratch whenever any speed effect changes, such as
> mounting, sprinting, a slow, or water. So a speed set from Lua would be wiped
> the first time the player mounts. The durable
> options are a hidden passive speed aura per race, or a small source patch to
> that recalculation.

more auras is more things to keep track of always. We gotta build a server patch
to add a short dispatch table that is placed between input and movement that
routes the function arguments to the right call site. Then, we can have one
walking method or function for each race and gender, with hard-coded at compile
time "default" movement speeds.

> - Square root: follows how real animals' walking speed scales with leg length.
> Gnomes end up around 0.57 and tauren gain less.

walking speed should scale with leg length and torso posture positioning

--------------------------------------------------------------------------------

### Assistant Response 4

> Rewriting the follow logic: one per-second check that either starts the
> server's own follow, or lets go once you've both stopped.

> The test passes 12 of 12; the old code would fail it, since it never used the
> server's follow. Recording the distance change in the balance log and updating
> the Sargobras issue to match.

*[commit] 1d8789d3b in wow-chat-2026 - The wandering Sargobras walks with you, behind you, and gives you some slack*

> Growth is a level-60 tauren druid, online right now. Next: listing Growth's
> buddies and checking how the existing buddy gear system picks their items.

> Gear has two possible routes that work differently, so I'll bring you that
> choice at the end of this turn. Meanwhile, updating the movement-speed issue
> with your decisions.

> Speed issue updated. Next is the starting-valley deck issue; reading how the
> rotation stores its valleys and sets the hearthstone, so the deck reuses the
> same data.

> Map issue written; implementing it waits on your choice of route. Back to the
> gear: checking that the Lua engine can equip items directly, since that
> decides whether the fast route works.

> **[Asked the user]**
>
> *Q1 — Gear route:* How should Growth's six buddies get their Molten Core-era
> gear?
> - Live, no restart (Recommended) — A generator scores every item that drops
>   in Molten Core or a classic dungeon (no Onyxia, Zul'Gurub, Blackwing Lair or
>   PvP) for each buddy's class and talent plan. A small Lua command then equips
>   the result on online buddies. It hot-reloads, so you don't recompile or
>   restart, and it works for any owner later.
> - Level-60 kit tier — Adds the same generated gear as a level-60 tier of the
>   buddy kit, so every buddy reaching 60 gets it. Then `.buddy kit <name>`
>   re-gears Growth's six. Needs your compile and a server restart. The kit is
>   one list per race and class, so a tank warrior and an Arms warrior would get
>   the same gear.
> - Both — Live command now for Growth's buddies, and the same generated gear
>   becomes the level-60 kit tier later.
> → **Answered:** make profiles that are for each buddy-bot type. Name them
> "elites". For now, just do the ones for Growth. But this should be an issue
> file. Include a checklist of each character, with roughly the same type of
> equipment as Growth has now. They represent the power of the Horde and
> Alliance, and are bannermen. For now, Growth's party is orcs, tauren, and
> trolls. So create profiles for them, and that'll be fine. Later on, we will
> develop more outfits for NPCs and apply the mirror-image trick like we did
> with Sargobras.
>
> *Q2 — Map route:* For hiding the map and minimap, which client-side route?
> - Addon (Recommended) — A small addon in the project, plus a script that
>   copies it into a client. Simple; a player can turn it off in the addon list.
>   An optional login hello lets the server notice when it's missing.
> - Client patch file — An extra Data/patch MPQ shipped with the client, out
>   of reach of the addon list. Harder to build, and untested: the 3.3.5a client
>   may reject edited interface files.
> - Hold for now — Leave it as a planned issue. One earlier basic-profile idea
>   was withdrawn with 'No restrictions on player behavior'.
> → **Answered:** an addon and no script that copies it to the client. We will
> offer it on our website and say it's the intended way to play unless you're at
> a planning table or a map, like outside the Dark Portal or in the throne
> rooms.

> Found 466 candidate items from Molten Core and the classic dungeons. Pulling
> their stats so each pick can be matched to the buddy's role.

> All 40 items confirmed in the database. Writing the profiles as a data file.

> Now the validator. Checking how existing Lua scripts in the project query the
> database, so this one matches.

> All 717 checks pass. Making sure the test can actually fail: planting a
> Zul'Gurub item, an Onyxia item and a cloth-for-warrior mismatch in a scratch
> copy.

> Writing the equip command. It works from in game (game masters only) and from
> the server console.

> Both files parse. Adding an offline check of the command itself to the same
> test: stand-in buddies with a two-hander and shield, a buddy with no elite,
> and a buddy who isn't in the world.

> All 727 checks pass. Noting the command check in the test's header comment,
> then moving to the map addon.

> Updating the map issue to record your decisions and what's now built.

I've done the Sargobras fix and the buddy gear, written the map addon, and filed
the starting-valley deck and movement speed as issues. The new gear and
Sargobras's new following only take effect once the server's Lua is reloaded,
which I've left to you, and nothing has been tried in game yet.

## Sargobras following (committed)

He was sent once a second, at his own running speed, to a spot 7 yards from
wherever you stood at that moment. So he sprinted, stopped, waited and sprinted
again.
- **Pace:** he now uses the server's own follow, which copies your walk or run
  on every step.
- **Position:** he stays about 10 yards behind you, on the side you came from.
  If you're owed several Sargobras, they spread out in a small arc behind you.
- **Slack:** when you've both stopped, he lets go. He only follows again once
  you're 25 yards away (it was 15). The distances are in the balance log.
- **Test:** `scripts/test-sargobras-follow` walks a stand-in you through each
  case and passes 12 of 12. The old code fails it.

## Growth's buddies: elites (issue `617r-elite-outfits`)

There are six elite profiles, one per buddy type in Growth's party. Each is the
class's Tier 1 set plus Molten Core and dungeon pieces picked for the buddy's
talent plan, roughly matching Growth's own gear. The issue has the checklist for
each character.

| Buddy | Elite | Weapons |
|---|---|---|
| Pozeta | Tauren warrior, Arms | Obsidian Edged Blade, Striker's Mark |
| Xazian | Troll warrior, tank | Brutality Blade, Drillborer Disk |
| Shauzhi | Tauren druid, Balance | Staff of Dominance, Idol of the Moon |
| Ecaexo | Troll mage, Frost | Azuresong Mageblade, Fire Runed Grimoire |
| Kilta | Orc shaman, Elemental | Staff of Dominance, Totem of the Storm |
| Muspo | Tauren shaman, Restoration | Aurastone Hammer, Observer's Shield |

`scripts/test-elite-outfits` checks every item against the world database. It
confirms each one:
- drops in Molten Core or a classic dungeon (found through the loot tables);
- needs no honor rank and requires level 60 or lower;
- fits its slot and can be used by the class.

All 727 checks pass. Planted Onyxia and Zul'Gurub items, plate on a mage, and a
two-hander with an off-hand were all caught.

**To put it on:** type `.reload ale`, then `.elite Growth`. The command replaces
each buddy's gear except shirt and tabard, and reports how many items each buddy
got.

The reload has a side effect: Growth's current wandering Sargobras loses track
of his owner. Relog Growth and a fresh one comes; the stale one stands where he
is until the next server restart.

## Map addon (issue `155y-no-map-no-minimap`)

The addon is in `src/client-addons/EverlandNoMap/`, with no install script.
- **Minimap:** hidden, always.
- **World map and zone map:** they close as soon as anything opens them, except
  outside the Dark Portal and in the throne rooms.
- **`/nomap where`:** prints the zone and sub-zone you're standing in. Several
  throne-room sub-zone names in its list are my guesses, so use this at each one
  to correct them.

It parses, but hasn't run in a client.

## Issues written

- **`155d1-another-valley-please`:** the starting-valley deck. Nothing is stored
  per character, the option only shows at level 1, and the hearthstone binds to
  every valley dealt. It also requires the eight valley positions to live in one
  place, since they're currently only written in the C++ patch.
- **`166-movement-speed-by-model-size`:** a server patch with a table indexed by
  race and gender, pointing to one walking function per body, each with a
  compile-time default speed. No auras. Height scaling comes first; the
  hips-and-shoulders gait version, using leg length and torso posture, is
  planned for the cleanup pass.

## Open questions (all in their issue files)

1. **Elites:** should an elite stay with a buddy after it's remade, or is it a
   one-time gift? Should elites become the kit's level-60 tier for every buddy?
   And do world drops that also fall in Molten Core, like Lady Maye's Pendant,
   count as Molten Core?
2. **Map:** should the minimap also come back at map places? Should anything
   from around it stay, like the zone name or mail icon? Does the no-map rule
   also apply in dungeons?
3. **Deck:** does a dealt valley count toward the faction's batch of 30 new
   characters?
4. **Speed:** linear or square-root curve for now? What does "farthest" mean? Do
   mounts keep body size? And should walking and swimming speed scale too, or
   only running?

Only the Sargobras fix is committed. The elites, the addon and the five issue
files are uncommitted, since your rules tie commits to finished issues.
`new-issue-please-sort` and `vision-mindscape` are still in the project root,
though their contents are now captured in issues.

--------------------------------------------------------------------------------

### User Request 5

also can you make the guards and townspeople in Outland be roughly scaled to the
threats they are to diminish? It should be an even fight, and one should come
through the portals with a random running trajectory that they adjust like a
compass toward a foe. Also there should be 3 different "sizes" of monster that
attacks. Imp size, character size + 10%, then overlord size with between
character size * 2 and 15. And they should continually attack, and the number of
allied fighters that spawns is about as many as can fit width wise through the
portal per second. They charge and attack the monsters that are attacking up a
narrow causeway. At any time, the player can take a gryphon or flying steed. The
characters are human and orc and other creature alike. Sometimes there's murlocs
and furlbogs who spawn. No Quilboar or Quiraji, the jerks. (they have other
things to focus on)

The paths that they pathfinding take are determined deterministically, with a
series of lines, curves, splines, running mostly forward. They cannot intersect,
and they must maintain a certain distance from one another. They cannot slow
down. They will avoid players, and the profile outfits we're making called
"elite" will sometimes spawn as hero units that help fight. But players are
pathfound around because the movement pattern is so deterministic - straight
ahead, right angle from the portal. They only fight monsters who are directly
in-front of them, and the monsters do the same. They will sort themselves to
fill the correct amount of space, with larger monsters always having priority,
but not displacing more than a few. They will attack what's directly ahead.
Spellcasters on the Horde and Alliance side will cast spells from afar (standing
still where there's sufficient flowrate to get past them) otherwise casting from
the front. Ranged casters will clump up ideally, causing melee to push forward
across their face. Because when you fight what's directly ahead, you can fight
those just to the side of them when you slay a foe. We will iterate on
successively more successful tactical planning degrees when we have more time to
talk. For now, fight to the last.

> To put it on: type .reload ale, then .elite Growth. The command replaces each
> buddy's gear except shirt and tabard, and reports how many items each buddy
> got.

didn't work? "Command "elite growth" does not exist

> The reload has a side effect: Growth's current wandering Sargobras loses track
> of his owner. Relog Growth and a fresh one comes; the stale one stands where
> he is until the next server restart.

should despawn according to his "finished talking to a player, should despawn
myself with these conditions" rules. If I remember correctly, he turns, walks
away at an angle, and sits at the fire that he lit underneath the player when
they chose an option. Oh by the way can we ensure that the fire he lights is
underneath the player? and make it increase in damage for each tier of buddy-bot
he's summoning.

also make sure he faces the fire before he sits. You can accomplish all of this
with rotations, angles, and high-school geometry.

Then he despawns after the fire does, and he becomes un-targettable to anyone
while he's sitting.

> 1. Elites: should an elite stay with a buddy after it's remade, or is it a
> one-time gift? Should elites become the kit's level-60 tier for every buddy?
> And do world drops that also fall in Molten Core, like Lady Maye's Pendant,
> count as Molten Core?

an elite doesn't get buddy-bots. They are just NPCs that help fight at the dark
portal. They don't belong to a player. Elites are the level-60ish tier (but make
sure they're specialized in stats for something they can technically use, but
which is sub-optimal, like mp5 on a resto druid) They should fight like the rest
of the buddy-bots, casting spells if they're ranged characters (only one type
per class) while clumping on one side of the cause-way. Priests can heal and
cast smite, druids can cast wrath but not moonfire and rejuvenation (which they
do to a random injured ally around them every 5 seconds or whatever) and other
such priorites. Please print a suggested table for me when the issue is
implemented.

> 2. Map: should the minimap also come back at map places? Should anything from
> around it stay, like the zone name or mail icon? Does the no-map rule also
> apply in dungeons?

nah no minimap ever

> 3. Deck: does a dealt valley count toward the faction's batch of 30 new
> characters?

let's say yes. you can always just loop back around, so there's no room for
griefing. Once your character is created, no other player's behavior can affect
you. But you affect others, if you choose to endlessly cycle the map for some
weird reason.

> 4. Speed: linear or square-root curve for now? What does "farthest" mean? Do
> mounts keep body size? And should walking and swimming speed scale too, or
> only running?

mounts scale with the size of the mount.

--------------------------------------------------------------------------------

### Assistant Response 5

> Starting with why `.elite growth` doesn't exist, since that's blocking you.
> Checking the server log for Lua errors from the reload.

> Now the guard test: it fails whenever two Lua files in a profile's script
> folder share a name.

> The name check passes, and it catches the old clash when I plant it again.
> `.reload ale` followed by `.elite Growth` should now work.
>
> On to Sargobras's campfire. Reading how his break works today.

> The server runs LuaJIT, so the new code's bit operations work. Adding the
> stale-Sargobras cleanup to his script's load, then writing the summon command.

> Everything loads cleanly and the console connection works. Reloading the
> server's Lua, then moving Growth's buddies.

> The hearthstone design is recorded. Now writing your other answers into their
> issues: the deck counts toward the batch, no minimap ever, and mounts scale
> with their own size.

> All 844 checks pass. "Also equip Talriah" reads as wanting the whole party
> geared, so I'm reloading the Lua to pick up the new profile and running
> `.elite Growth` for all seven.

> All 18 proficiency spell numbers are confirmed. Checking that the Lua engine
> can read an item's kind at runtime.

> Updating the command's offline test for the new behaviour, with stand-ins for
> proficiencies, dead buddies and putting the old item back.

> All 849 checks pass. Now live: reviving Ecaexo from the console, reloading the
> Lua, and dressing the party again.

> All seven of Growth's buddies are now fully dressed, Talriah included. Writing
> the proficiency gap into the buddy spell-learning issue.

> Recorded. Next is the Dark Portal battle issue. First, checking how Outland is
> already scaled for basic and how the creature-strength system works, so the
> guard scaling builds on it.

> The parent issue is written. Now the seven sub-issue files, each a blueprint
> with current behavior, intended behavior and steps.

*[commit] 2bfe258cd in wow-chat-2026 - Sargobras lights his fire under you and leaves with it; a reload no longer strands him*

All seven of Growth's buddies are now dressed, Talriah included, and the six
that were away are back beside Growth. Sargobras's new fire-and-sit behaviour is
live but not yet seen in game. The Dark Portal war is written up as issues only;
none of it is built.

## Buddies back after your hearthstone

All six were moved to you from the server console, onto a ring around you,
facing in. They're still at level 60. This used a new game-master command,
`.buddies here <name>`, which you can run yourself next time.

The real fix is recorded in the buddy-adventuring issue: a buddy casts its
hearthstone, then is placed at its own stored spot and facing on a ring around
where you bound.

## `.elite` and Talriah

**Why the command didn't exist:** the server's Lua loader refuses a second file
with the same name, even in another folder. My data file was also called
`elite-outfits.lua`, so the command script never loaded. I renamed the data file
and added `scripts/test-lua-unique-names`, which fails whenever two script files
share a name.

**The first run went badly** for three reasons:
- **Missing proficiencies.** These buddies were made straight at level 60 and
  never learned what a trainer or weapon master teaches. The warriors had no
  Plate Mail, bows or guns; Kilta had no Mail; Ecaexo had no one-handed swords.
  This gap is now in the buddy spell-learning issue.
- **Ecaexo was dead.** The dead can't change gear.
- **My mistake:** the command took off old items before checking the new ones
  would go on. The starting whites in the refused slots were lost; the second
  run filled every one of those slots.

**What I changed:**
- The command now teaches a missing proficiency before equipping and reports
  each one.
- It skips dead buddies and says so.
- It puts the old item back if a slot refuses the new one.

I revived Ecaexo and added a "Blood Elf Paladin (damage)" profile for Talriah.
The second run dressed 7 of 7. Talriah wears Lawbringer (the paladin Tier 1 set)
with Obsidian Edged Blade; my first pick, Spinal Reaper, failed the validator
because nothing in Molten Core or a dungeon drops it. The validator now runs 849
checks, all passing.

## Sargobras (committed)

The reload already put this live:
- The fire is lit under the chooser's feet.
- It burns 2% of max health per second for each tier he serves. The player deals
  it to themself, so he's never in a fight.
- He walks off at an angle to a seat 3 yards from the fire, turns to face it,
  then sits. He can't be targeted while sitting.
- He leaves when the fire goes out, after 120 seconds.
- On every reload, his script removes ownerless Sargobras near players and
  summons fresh ones for anyone owed a buddy.

The offline test passes 27 of 27. The 120 seconds and 2% are first guesses,
recorded in the balance log.

## Dark Portal front (issue `155z-dark-portal-front`, seven sub-issues, not built)

Your full message is quoted in the parent issue. The parts:

1. Outland's guards and townspeople raised to the median level of the hostile
   creatures around their own spawn point.
2. Deterministic lanes: lines, arcs and splines that never cross, keep a minimum
   gap, never slow, and bend around players.
3. Reinforcements through the portal, as many per second as fit across its
   width, each turning like a compass toward a foe.
4. The demon tide in three sizes; larger demons claim lanes first but displace
   only a few.
5. Fighting only what's directly ahead; casters clump; fight to the last.
6. Elites as hero NPCs. I'll print the suggested elite table (class, role,
   off-best stat, one ranged spell kind, spell priority) once this part is
   built, as you asked.
7. A flight always at hand.

The order I suggest:
- Part 1 first; it's only database data, so no compile.
- Then the lanes, drawn as an animation for you to look at before any server
  code.
- Then the server code.

## Answers recorded

- **Deck:** dealing a valley counts toward the batch of 30, and your own deck
  order never depends on anyone else's cycling.
- **Map:** no minimap, ever.
- **Speed:** a mounted player moves at the mount's size, not their own.

## Still open

1. **Elite name:** elites are now the Dark Portal's hero NPCs, but the tool that
   dressed Growth's buddies is also called "elite". Rename it, for example to
   `.outfit`?
2. **Dark Portal:** does "portals" mean only the Dark Portal? Which way does the
   causeway run? Are the murlocs and furbolgs allies or a third side? Do kills
   there give experience or loot? What's the lane gap and the size of "a few"? A
   flight master that stays clear of the battle, or a flight you can summon
   anywhere?
3. **Speed:** linear or square-root curve? What does "farthest" mean? Should
   male and female differ? Every profile, or some? Walking and swimming too?
4. **Map addon:** six throne-room names in its list are guesses. Use `/nomap
   where` at each one to confirm or correct them.
5. **LLM compute issue:** the six questions in that issue file are still open.

Uncommitted, because their issues are still in progress: the gear command and
its data, the map addon, the elite issue, and the deck, speed, LLM-compute, map
and Dark Portal issues.

--------------------------------------------------------------------------------

### User Request 6

can we add a command for gms that teleport all of a target player's buddy-bots
to them, and also, another one that teleports all of the target player's
buddy-bots to them, the GM?

also another gm command that revives all of a target player's buddy-bots, both
with options to revive them at the GM's location, the player-partner's location,
and the nearest graveyard. In all situations they should be placed with an arc
around a circle with a certain radius. The arc should be modified depending on
how many buddy-bots are registered to the player. The player's should often
count as one, but sometimes the player-partner is the center.

Also can we make it so that when we assemble random dungeon groups dynamically
from buddy-bots in a clan, that we always ensure there's a tank and healer
present? We can count half-specs and third specs as 0.3 for the role - half and
third both are 0.4 let's say, so you need at least 1 healer for example, or 3
half healers. For raids, all of your characters join you so...

We're doing a test dungeon HFR and our characters are equipped with Molten Core
gear. We should be able to address the challenges, but unfortunately the players
have too little health. Can we scale the stamina on gear a bit harder? WotLK is
notoriously low-health, rapid healing requirements, so things like Spellpower
are useless for healers and only sustained pressure is required. Blizzard buffed
health in Cataclysm, the next expansion, but they had to overcorrect a bit later
on. So, let's try something similar, except we should scale the difficulty curve
(calculated from things like combat attack damage and spell damage from NPCs
multiplied by effective DPS of the enemy groups) according to the HTML document
we created recently listing the equipment ratios per item level post-scaling.
Except, I can't find the HTML page now... Lemme try looking in my browser
history NOOOOO haha okay.

--------------------------------------------------------------------------------

### Assistant Response 6

> Starting with the missing HTML page, since that's a quick search: looking
> through the project for a page about equipment ratios per item level.

> Nothing in the project matches, so it may have been published as an artifact.
> Checking your artifact list and searching the recent transcripts.

> The likely match is **"Level-60 Gear Ladder"**, published 2026-09-25. Reading
> it to confirm it holds the per-item-level ratios.

> Your rule says the issue comes before the code, so I'm writing the
> weighted-role rule into the dungeon-draw issue first.

> The rule is recorded. Now the code in the buddy module. You'll compile it, so
> I'll check it against the build's own flags before handing it over.

> The placement geometry passes all 168 checks. Now the command script itself,
> replacing the earlier `.buddies here`.

> All 13 checks pass. Running the graveyard search against the real world
> database once, removing the old `buddies-here.lua`, and fixing every reference
> to it.

> The commands are written and pass their tests. Reloading the server's Lua so
> they're live, then checking with the harmless help form only, since your party
> is in a dungeon right now.

> Writing the weapon-specialization idea as an issue first, since there's a
> scope question in it.

> All valid. Committing the buddy commands and the party draw change; the new
> issue files stay uncommitted under your rule.

*[commit] 73dcf6e16 in wow-chat-2026 - Game masters can gather, summon and revive a partner's buddies; dungeon draws count part-healers and part-tanks*

The three game-master buddy commands are live and committed. The tank-and-healer
rule for dungeon draws is written but needs your compile. Stamina and the
weapon-specialization switch are written up as issues, each waiting on your
answers.

## Your HTML page

It's the artifact **"Level-60 Gear Ladder"**:
https://claude.ai/artifact/MZ1mumH84gVxVicSnskvD8. It's linked from the new
stamina issue so it can't get lost again.

## Game-master buddy commands (live, committed)

| Command | What it does |
|---|---|
| `.buddies to partner [name]` | Brings a partner's buddies to the partner |
| `.buddies to me [name]` | Brings them to you, the GM |
| `.buddies revive [name] at me / partner / graveyard` | Revives only the dead ones at that place |

- **Who:** with no name, it uses your selected target, and selecting a buddy
  means its partner. On the console, give a name; "me" doesn't work there.
- **Graveyard:** the one nearest the partner that their faction may use. Checked
  on the real database: from Thrallmar it finds the Thrallmar graveyard.
- **Placement:** three times in four, the anchor (you, the partner or the
  graveyard) is one spot on the circle, with the circle's centre in front of
  them. The rest of the time, the anchor stands in the middle and the buddies
  form an arc in front. Add `ring` or `centre` to choose.
- **Stable spots:** the circle's size comes from how many buddies are
  registered, and each buddy keeps the spot for its roster slot, so the shape
  doesn't shift depending on who's present.
- **Tests:** 168 checks on the geometry and 13 on the commands, run offline with
  stand-ins, all passing. In game I only tried the help text, since your party
  is mid-dungeon.

These replace `.buddies here`.

## Tank and healer in dungeon draws (written, needs your compile)

The group now has to reach a full tank and a full healer as sums:
- A full one counts 1.0.
- A buddy with a third or two-thirds of its talent plan in a tree of that role
  counts 0.4, so three part-healers make 1.2.
- The draw takes whoever adds the most first.

It passes a syntax check with the build's own flags.

## Stamina (issue `155k1-stamina-to-the-threat`, not built)

The method follows your description:
1. For each row of the gear ladder, measure the enemies' damage per second for a
   typical pull: melee from the server's per-level damage table times each
   creature's damage modifier, plus spell damage.
2. Compare that with the health a character in that tier's gear has.
3. Scale each item's stamina by its stat-budget level so everyone reaches a
   chosen "seconds to die with no healing".

The first step is a measuring tool, then a page to pick the target on, then the
item rescale with an exact revert. I'd like your answers before building:
1. One target number of seconds, or one per role, with tanks longer?
2. Melee only at first, or estimate spell damage from the start?
3. All gear, or only level 60 and up?

## Weapon specialization (issue `617g1-weapon-specialization-follows-the-weapon`, not built)

When an Arms warrior or Combat rogue equips a main-hand weapon, the points in
its weapon-kind talents move to the matching one. The count and tier stay the
same, so nothing else in the tree locks or unlocks. Questions:
1. Buddies only, or players too?
2. Also in combat, or only out of combat?
3. Should a dual-wielding rogue follow only the main hand?

## Also open

- **Raids:** "all of your characters join you so…" trailed off. Today nothing
  is drawn into a raid. Should entering one bring every online buddy of every
  player in the raid, and must the raid also reach a tank and healer sum, maybe
  one per five?
- The questions from earlier this session are still waiting: renaming the gear
  tool, the Dark Portal details, the speed curve, and the throne-room names for
  the map addon.

--------------------------------------------------------------------------------

### User Request 7

also can we make buddy-bots have an aggro radius in dungeons? They should attack
when they are ~10% farther than the monster's aggro radius. Otherwise, they
follow the player.

--------------------------------------------------------------------------------

### Assistant Response 7

> Checking what already exists: an aggro-radius issue (142), how buddies behave
> inside instances today, and whether the Lua engine can read a monster's aggro
> radius.

> Writing the in-dungeon aggro check into the module's world pass, with its own
> half-second timer, since the strategy pass only runs every three seconds.

> Syntax-checking it with the build's own flags.

*[commit] a4570e814 in wow-chat-2026 - Inside dungeons, buddies strike monsters just before the monsters would notice them*

The in-dungeon aggro radius is written and committed but not built yet; it needs
your compile and a server restart, along with the earlier tank-and-healer
change.

**How it works:**
- Every half second, a buddy inside a dungeon or raid that isn't already
  fighting looks for the nearest monster that would attack it on sight. That
  means hostile, aggressive, not already fighting, and in line of sight.
- Once that monster is within 110% of its own aggro radius for that buddy, the
  buddy opens the fight, the same way a roaming buddy does: it targets the
  monster, switches to combat, and a melee buddy runs in.
- Otherwise it keeps following you.
- The radius is the server's own for that monster-and-buddy pair: 20 yards,
  minus a yard for each level the buddy is above the monster, kept between 5 and
  45.

It passes a syntax check with the build's own flags. Each strike is written to
the buddy's debug trace as monster, distance, radius and multiplier, so you can
see in a test run why a buddy pulled. The decision is recorded in the
dungeon-draw issue.

Two things to decide:
1. It also applies in raids, since the server counts a raid as a dungeon. Should
   it be five-player dungeons only?
2. It ignores neutral monsters, which only fight when struck first, so those
   stay for you to pull. Should buddies strike those too?

--------------------------------------------------------------------------------

### User Request 8

sometimes in dungeons, bots just sorta stand around. What playerbots module
addon plugin commands are there to direct them in dungeon group contexts?

--------------------------------------------------------------------------------

### Assistant Response 8

> Checking the project's playerbots docs and the earlier command survey from
> July rather than answering from memory.

You direct them by typing commands in party chat (`/p`), or whispering one bot
by name. The full list is in `docs/playerbots/Playerbot-Commands.md`, with
ready-made macros in `Playerbot-Macro-Collection.md`. These are the ones that
matter in a dungeon.

**Moving and fighting:**

| Command (in `/p`) | What it does |
|---|---|
| `attack` | Everyone attacks your selected target |
| `follow` | Run to you |
| `flee` | Run to you, ignoring everything else |
| `stay` | Hold position |
| `summon` | Bots teleport to you |
| `reset` | Drop whatever they're stuck doing (casting, moving) |
| `release` / `revive` | Release spirit, or revive near a spirit healer |
| `co +grind` | Attack any visible target, then the next |
| `co +assist` / `co +aoe` | One target at a time, or many |
| `co +passive` / `co -passive` | Don't fight / fight again (good for clean pulls) |

**Picking who hears a command:** start it with `@tank`, `@heal`, `@dps`,
`@ranged`, `@meleedps`, `@group2` or a class like `@warrior`. For example,
`@tank attack`, or `@heal co +passive`.

**Focus targets:**
- `rti skull` sets the raid icon they focus.
- `attack rti target` sends them at it.
- `rti cc moon` sets the target to crowd-control.

**A clean pull** (from the macro file): `/p @dps co +passive`, `/p @heal co
+passive`, then `/p @tank attack`, then eight seconds later `/p @dps co
-passive` and `/p @heal co -passive`.

**Checking state:** `co ?` and `nc ?` make a bot say which combat and non-combat
strategies it's running.

**Addon:** MultiBot puts most of these on buttons. Its docs recommend the fork
at github.com/Wishmaster117/MultiBot.

## A catch with buddies

Every 3 seconds, our module re-applies a fixed set of non-combat strategies to
each buddy inside an instance: `follow` on, `grind` off, `loot` on, roaming and
travel off. So anything you type that changes those (`nc -follow`, `nc +stay`,
`co`/`nc` grind) gets undone within 3 seconds. Combat strategies (`co …`) and
one-shot commands (`attack`, `reset`, `summon`) aren't touched.

## Why they stand around

When the bot module's bots follow a leader, they only fight what the leader
attacks or what attacks the group. A monster that hasn't noticed anyone yet, and
that you haven't hit, isn't a fight to them. So if you don't swing first, they
wait.

The aggro radius you asked for last turn is aimed at exactly this, once it's
compiled: each buddy strikes a monster that's within 110% of that monster's
aggro radius.

A question back, to narrow it down: when they stand around, is there a monster
in view that hasn't aggroed, or is it after a fight ends with nothing nearby? In
the first case the aggro radius should fix it. In the second, they're probably
stuck mid-follow, and `/p reset` then `/p follow` would get them moving.

--------------------------------------------------------------------------------

### User Request 9

Can we add a command that uses the same targetting rules to send one bot to a
location we specify with our mouse?

> Every 3 seconds, our module re-applies a fixed set of non-combat strategies to
> each buddy inside an instance: follow on, grind off, loot on, roaming and
> travel off. So anything you type that changes those (nc -follow, nc +stay,
> co/nc grind) gets undone within 3 seconds. Combat
> strategies (co …) and one-shot commands (attack, reset, summon) aren't
> touched.

whoa 3 seconds? That seems like a bad design.

> The aggro radius you asked for last turn is aimed at exactly this, once it's
> compiled: each buddy strikes a monster that's within 110% of that monster's
> aggro radius.

walks into range and attacks / spellcasts when the player is at 110% aggro range
of the monster.

Also, can we change their following behavior? They should form a circle around
the player, using the same method we used for the innkeeper hearthstone
placement system. Except, the waypoints should update around the player. Each
buddy-bot should always (depending on group) find themselves at the same general
"compass" direction, projected onto the fewer slots with fewer party members. So
if there's 7 buddy bots for a total of 8 characters, the player-partner in the
middle, then the 7 buddy-bots might each have a direction like "north" or "west"
or whatever. They should always move toward this location around the player.
Also can we add a command that makes this radius slightly farther out, and
another that brings it in to a minimum? The "farthest out" they can be is 40
yards, and it should respect the terrain and doodads and such. This is only in
follow mode. Also, the tank will displace whoever is closest to where the player
is facing, and they will adjust to find the closest orientation to their anchor
spot. This will mean that a tank and a character to their side on the compass
will switch positions when the player-character turns. The goal is to keep the
tank directly in-front of the player, so when the player moves the tank
character close enough to the foes... They will charge forward at 110% of the
monster's aggro radius. We can disable that behavior when the character is in
combat so we're not attempting to re-orient all the time. And when a fight is
underway, the playerbots will move around, out of AoE's and such... Sometimes
just moving to move, getting a different angle on the target, usually about 10
or 15% of the arc length of the circle created around the target with radius
equal to the distance to the target. Again, always respecting the terrain
pathfinding navmesh.

--------------------------------------------------------------------------------

### Assistant Response 9

> Starting with the 3-second reset, since you're right that it's a bad design.
> The pass doesn't just set a buddy's strategies when it enters a new kind of
> place. It re-checks them every 3 seconds and puts back anything that differs,
> which means it treats your own commands as drift to correct.
>
> The fix: apply the set only when a buddy's situation changes (into or out of
> an instance, a town, or the open country, or when it first comes online), and
> leave it alone in between. Reading that code again.

> Both changes pass the syntax check. Correcting the aggro decision in the issue
> to say your distance is the trigger, and recording the strategy-set fix.

*[commit] bc0260d8b in wow-chat-2026 - Buddies keep the orders their owner types, and go at a monster when the owner nears it*

I fixed the 3-second reset and changed the aggro trigger to your distance; both
are committed and need your compile. The compass formation and the
server-to-client tooltips are written up as issues; nothing for them is built
yet.

## Fixed (committed, needs your compile)

- **The 3-second reset:** buddies now get their strategy set only when they
  change kind of place (open country, town, instance) or come back online.
  Commands you type, like `nc -follow` or `nc +stay`, now stand until then.
- **Aggro trigger:** it's now your distance. When you come within 110% of a
  monster's aggro radius for you, every buddy of yours inside that isn't
  fighting goes at it: melee runs in, casters move into range and cast. Each one
  writes the distances to its debug trace.

Both pass a syntax check with the build's own flags.

## Compass formation (issue `617s-compass-formation`, six sub-issues, planned)

1. **Compass slots:** each buddy keeps its own direction (from its roster slot).
   Each present buddy is matched to the free spot on the ring closest to its own
   direction, so with fewer present each still keeps roughly the same direction.
   This part is pure geometry, tested offline and drawn as an animation first.
2. **Ring follow:** out of combat, in follow mode, each buddy keeps moving
   toward its spot as you move. A spot behind a wall or off a ledge slides in
   toward you until it's walkable.
3. **Tank at the front:** the tank always takes the spot you're facing,
   displacing whoever held it, so the tank swaps with a neighbour when you turn.
4. **Radius commands:** one widens the ring a step at a time, up to 40 yards;
   the other brings it back to the minimum.
5. **Sidesteps in a fight:** a buddy now and then moves 10–15% of the way
   around the circle about its target, staying at the same distance from it.
6. **Send one buddy:** one buddy goes to a spot you click, chosen by name or
   `@tank`-style. The bot module's own click-to-save-a-spot command (`rtsc`)
   does nearly this already; the open question is whether to reuse it.

The questions that decide the build:
1. What's the default radius, the step size, and the minimum?
2. Are the compass directions fixed to the world, with only the tank's front
   spot turning with you?
3. Do only buddies in your group take spots?
4. Command names: `.buddy wider` and `.buddy closer`, usable by any player for
   their own buddies?
5. Reuse `rtsc` for the click, or make our own spell?
6. How often should sidesteps happen, and only for melee?

## Ability tooltips (issue `719-true-ability-tooltips`)

The roadmap says no AIO, and no feature may depend on an installed addon. So
this is the "something similar", kept within those rules:
- **The server** works out each ability's real numbers for the character:
  damage, healing or absorb after spell power or attack power, talents and
  auras, with no target applied.
- **It sends them** over the client's addon-message channel, at login and
  whenever gear, auras, talents or level change, at most once a second.
- **An optional addon** adds a line under the normal tooltip, for example
  "Everland: 412 to 480 Fire damage (base 300 to 350)". Without the addon,
  players see the normal tooltip.

The calculation needs a server module in C++, because the Lua engine can't reach
the server's bonus math. The addon appends a line rather than rewriting the
tooltip's own numbers, because it only sees the finished text.

Questions:
1. Damage and healing first, or also cooldowns, durations and costs?
2. Target-free numbers only, or also an estimate against your current target?
3. Abilities only, or item tooltips too?

The new issue files are uncommitted, as before.

--------------------------------------------------------------------------------

### User Request 10

can we make a patch based on the one we have already that makes death knights
have an option when talking to Sargobras to skip the death knight story? They
otherwise can talk to Arthas for the first DK quest.

> 2. Ring follow: out of combat, in follow mode, each buddy keeps moving toward
> its spot as you move. A spot behind a wall or off a ledge slides in toward you
> until it's walkable.

we should make it so that the waypoints only move when the buddy-bot reaches
one.

> 6. Send one buddy: one buddy goes to a spot you click, chosen by name or
> @tank-style. The bot module's own click-to-save-a-spot command (rtsc) does
> nearly this already; the open question is whether to reuse it.

we should be able to say @party or @name to pick one specifically and @1 or @2
to select the first or second character in your party and also we should be able
to do as many as we want at the same time. Then, ideally, an issue file in the
custom client to allow us to select them by clicking and dragging a box.

>   1. What's the default radius, the step size, and the minimum?

5, 5, 25

> 2. Are the compass directions fixed to the world, with only the tank's front
> spot turning with you?

yes fixed to the world, but dynamically adjusting. Maybe we could talk to
Sargobras (or a second, assistant NPC) to enable or disable that behavior.
Chromie?

so that it'd give us config options to change for certain server behaviors.

>   3. Do only buddies in your group take spots?

yes. can we also make issue files to make it so that if you have fewer than 4
buddy-bots, if you enter a dungeon, you get companions that are equipped with
the level appropriate white gear randomly selected and created? They should be
socketed according to their class styles. Also make sure that we have an item
for every spot, and when available give them PvP trinkets, rings, and necklaces,
the best available at that level. Also we need 1 tank, 1 healer, and the rest of
the profiles treated as DPS, which is different than the default rule which has
exceptions for half-heal/tank specs and third-heal/tank specs.

Also can we put meta gems on white quality weapon items? Not sure how to acquire
them yet... We should mark it as an open question.

Do you think it'd be alright to examine the LLM bot conversation system in
detail? Especially regarding it's dataflow from the user's perspective. I will
ask if I want technical details.

> 4. Command names: .buddy wider and .buddy closer, usable by any player for
> their own buddies?

yes, ideally with pre-made macros if we can figure out how

>   5. Reuse rtsc for the click, or make our own spell?

let's see both implementations to see what they each do differently. Try to
distinguish yourself from their implementation in as many ways as you can, so we
can see the pattern of behavior.

>   6. How often should sidesteps happen, and only for melee?

melee should do a larger proportion of the circle each time. How about 20-35%
for melee and 10-20% for ranged? We can also consider just scaling it linearly
from 0 to 40 yards away, with 40 yards being 2-3 yard adjustment and 3-0 yards
away being between the character's melee attack range (maybe we should modify
for model size too?) and the target's center?

  - An optional addon adds a line under the normal tooltip, for example
    "Everland: 412 to 480 Fire damage (base 300 to 350)". Without the addon,
    players see the normal tooltip.

nope ignore the original number, just the calculated one. We want to prove the
value of resistances on foes, and switching your attack patterns to address
them. Armor values too. Also Resilience A+ GOLD STAR

>   1. Damage and healing first, or also cooldowns, durations and costs?

yes

also... do we have haste rating for gemstones? If so, excellent!

> 2. Target-free numbers only, or also an estimate against your current target?

I suppose we could send the target's resistance values, and the client could
calculate it on the spot. But then we'd want to show both numbers. And you know,
all you have to do is cast the spell and see what the number is in order to know
the target's resistance. It's kinda better that way actually. So, no, don't
estimate the damage to the target. Let them figure it out themselves.

>   3. Abilities only, or item tooltips too?

how would we do items? Do you mean like weapons showing their DPS? I think we
should not do that actually, because it allows players to compare strategize
when the values are constant. So, no items, unless you can think of some
specific situations. Abilities only need be compared against each other, while
stats have to be measured and balanced.

can we add a patch that makes spirit restore hp5 equal to the amount of mana it
would restore if the character wasn't casting?

--------------------------------------------------------------------------------

### Assistant Response 10

> Several requests here, so I'm tracking them as tasks. Starting with the death
> knight skip: looking for the existing patch you mean and how death knights
> meet Sargobras today.

Quests are added one at a time and rewarded in order, so the quest log never
fills up. Pulling the Acherus quest list and their order from the world
database.

--------------------------------------------------------------------------------

### User Request 11

Base directory for this skill: /home/ritz/.claude/skills/upstream-patch-system

# Upstream Patch System

A method for owning customisations to code you didn't write, without forking it.

This describes machinery to build. Everything is plain shell, git and one
small LuaJIT helper; working reference versions ship in this skill's
`scripts/` directory (see "The parts that ship with this skill"). Where the
owner of the project has standing instructions that differ from anything here
(where temporary clones live, what language helper scripts are written in,
whether each patch gets its own issue file, how scripts are laid out), their
instructions win; follow them and adapt the machinery to fit.

---

## Principle: machinery, not procedure

A **procedure** is a sequence someone must remember to work through. A
**machine** is components wired so the outcome falls out with no one's memory
in the loop. This system is built as machinery on purpose:

- **Every "remember to…" is a defect.** Convert it into one of four forms: a
  **guard** (a check that makes an operation a no-op when it shouldn't fire), a
  **trap** (cleanup wired to process exit, so it runs even on failure), a
  **generated artifact** (a derived file that can't drift because it is only
  ever recomputed), or a **gate** (a hard stop until a condition or an approval
  is satisfied).
- **The only inputs left to a human are irreducible.** Three: the
  *transformation itself* (the old→new edit and the anchor that locates it),
  the
  *judgment that a patch belongs upstream*, and the *approval of an outward
  action* (opening a PR, deleting a fork). When patches apply, when they revert,
  whether they still match, what the registry says, whether the tree is clean
  —
  all of that is wired.

---

## The problem this solves

You depend on third-party code and need to change it: fix a build error against
your toolchain, add a hook, silence warnings, tune a constant. The two naive
approaches both rot. **Editing in place** gets clobbered on the next pull, or
silently survives and is forgotten. **A hard fork** turns every upstream update
into a merge conflict until "what did we change and why" is unanswerable.

The third path: **the cloned source tree is a disposable build artifact**,
never committed and never the source of truth. The truth is a directory of
small scripts, each knowing how to *apply* one customisation and how to
*reverse* it. What you version is the intent (the scripts and their headers),
not the result; the result is a deterministic function of
`pinned upstream commit + scripts`.

What it buys: upstream updates stop hurting (a patch that no longer matches
fails loudly and names itself); every change is documented where it is made,
and the registry is generated from those headers; the diff is N small named
edits instead of one sprawling fork diff; changes replay onto any compatible
upstream.

---

## A patch is a reversible function over a text tree

```
apply:    pristine_tree    ->  customised_tree
unapply:  customised_tree  ->  pristine_tree     (exact inverse)
```

Two properties, both enforced mechanically:

1. **Idempotent.** Applying twice equals applying once; same for unapply. The
   enforcement is the **witness guard**, and it is the *first line* of every
   apply and every unapply: apply returns immediately if the patch's marker is
   already present; unapply returns immediately if it is absent. Re-running is
   inert by construction, not by care.
2. **Exact inverse.** `unapply(apply(tree)) == tree`, asserted by the verifier
   (below) against a clean tree, never trusted.

---

## The tiers: separated by when they fire and what they touch

```
PRE-BUILD   (source patches)   after clone, before compile   edits the SOURCE tree
POST-BUILD  (install patches)  after install to staging      sets up the INSTALL tree
POST-DEPLOY (config patches)   after promote to live         tunes the LIVE config
```

The split is chronological because each tier needs something the previous phase
creates: PRE-BUILD needs source present and no binary yet; POST-BUILD needs the
binary installed into staging; POST-DEPLOY needs the live directory to exist so
it can write paths and credentials that point at it. A side benefit: runtime
tuning written after the validation gate cannot fail validation, because
validation runs against complete staging configs.

**PRE-BUILD is the load-bearing tier** — the only one that touches code you
don't own, so the only one whose revert must be exact. The others touch your own
artifacts and behave like ordinary idempotent setup. Wire only the tiers your
chronology needs.

---

## Directory layout

```
project/
├── patches/
│   ├── patches.sh             # project settings, per-profile lists, sources
│   │                          #   every patch file, then scripts/patches-lib.sh
│   ├── P001-<short-name>.sh    # PRE-BUILD patch: probe + apply + unapply
│   ├── I001-<short-name>.sh    # POST-BUILD patches
│   └── C001-<short-name>.sh    # POST-DEPLOY patches
├── scripts/
│   ├── patches-lib.sh         # drivers: apply, unapply, reset, lock, audit
│   ├── replace-exactly.lua    # literal find-and-replace with a count check
│   ├── scaffold-patch         # generator for a new patch file
│   ├── verify-patches         # the gate: idempotency + round trip, per patch
│   ├── gen-registry           # regenerates the registry from headers
│   ├── compile                # the build driver (below)
│   └── update-upstream        # moves the pin forward (below)
├── docs/patches/
│   ├── patch-registry.md      # GENERATED — never edited by hand
│   ├── audit-<commit>.md      # one report per upstream update
│   └── <feature>.md           # optional prose "patch docs"
├── .upstream-lock             # the pin: "<repo> <commit>" per repository
└── source/                    # THE CLONED TREE — gitignored, disposable
```

Prefixes (`P`/`I`/`C`) are arbitrary; one tier per prefix, numbered in order, so
a glob selects a tier. The load-bearing line of the whole design is `source/`
in `.gitignore`: a surviving modification to upstream code in your history is,
by construction, a bug.

---

## The standard patch (marker-wrapped, witness-guarded)

`scaffold-patch P <short-name> <path-under-source>` writes this skeleton with
the next free ID, the functions named, and the marker filled in. The author
writes only the old text, the new text and the header's reasons:

```bash
#!/usr/bin/env bash
# P017-aggro-radius.sh
#
# TARGET:   src/server/game/Creature.cpp :: GetAttackDistance
# SYMPTOM:  aggro radius shrinks for higher-level players
# CHANGE:   symmetric radius around equal level, because <reason>
# ANCHOR:   float retDistance = 20.0f;
# MATCHES:  1
# MARKER:   P017 aggro-radius

# {{{ patch_needs_applying_P017
patch_needs_applying_P017() {
    local FILE="${SRC}/src/server/game/Creature.cpp"
    require_file P017 "${FILE}"
    ! grep -qF ">>> P017 aggro-radius BEGIN" "${FILE}"
}
# }}}

# {{{ patch_P017_aggro_radius
patch_P017_aggro_radius() {
    local FILE="${SRC}/src/server/game/Creature.cpp"
    patch_needs_applying_P017 || return 0          # witness guard, always first
    replace_exactly "${FILE}" 1 3<<'OLD' 4<<'NEW'
    float retDistance = 20.0f;
    retDistance -= static_cast<float>(levelDiff);
OLD
    // >>> P017 aggro-radius BEGIN
    float retDistance = SymmetricAggroRadius(levelDiff);
    // <<< P017 aggro-radius END
NEW
}
# }}}

# {{{ unpatch_P017_aggro_radius
unpatch_P017_aggro_radius() {
    local FILE="${SRC}/src/server/game/Creature.cpp"
    patch_needs_applying_P017 && return 0          # nothing of ours here
    restore_marked_block "${FILE}" "P017 aggro-radius" 3<<'OLD'
    float retDistance = 20.0f;
    retDistance -= static_cast<float>(levelDiff);
OLD
}
# }}}
```

What the shape guarantees:

- **The old and new text are quoted heredocs handed to a literal matcher.**
  `replace_exactly` (a wrapper over `replace-exactly.lua`) compares characters,
  not a pattern, so nothing in upstream code needs escaping — the hand-escaped
  `sed -z` regexes of earlier versions broke on every `.`, `*`, `[` and `&`. It
  also refuses to edit unless the old text occurs exactly the stated number of
  times: zero means upstream changed the code (the patch is stale), more means
  the anchor is ambiguous.
- **The inverse never re-matches the patched content.** `restore_marked_block`
  replaces the lines from `>>> <label> BEGIN` through `<<< <label> END` with the
  original text (or with nothing, for a patch that only inserted lines).
  Upstream
  reflowing around the block can't break it, and a sibling patch can't hit it.
- **The marker is the witness.** The probe, the guard and the inverse all key
  on the same string, so they agree by construction. The markers use
  `>>> … BEGIN` / `<<< … END` rather than editor fold markers, so they don't
  fold up or mis-nest inside the upstream code they sit in.
- **A pure insertion** puts the anchor line into both OLD and NEW (NEW is the
  anchor plus the marked block), and its unapply restores an empty block.
- **A patch that edits two places** uses two labels (`P026 engine-guard`,
  `P026 login-fix`), each with its own guard, and a probe that reports "needed"
  if either marker is missing.
- **Missing target files are an error**, not a skip: `require_file` stops the
  run and calls the patch stale. A patch whose target legitimately doesn't exist
  in some profile says so in its probe instead —
  `[[ -f "${FILE}" ]] || { echo " [P017] skipped: target absent (optional in
  this profile)"; return 1; }`
  — so the skip is declared and announced, never silent.
- **Single-line edits may use a `sed` line address** (`sed -i '/anchor/a\ …'`,
  `/anchor/i\`, `/anchor/c\`) inside the same guard and markers, the way many of
  the reference project's patches do. The guard and the marker-range inverse are
  what matter; the literal matcher is for multi-line text.

**Detecting convergence.** Because upstream never ships your marker, the
witness can't tell you when upstream adopted your change. A patch can define an
optional `converged_P017` function that checks for the new behaviour *without*
markers (for example, `grep -qF "SymmetricAggroRadius" "${FILE}"`); the audit
uses it to tell RETIRE from STALE.

---

## The build driver

The project's `scripts/compile` wires the pieces so each step guarantees what
the next relies on:

```bash
set -euo pipefail                            # a failed step stops the run
source "${DIR}/patches/patches.sh"           # settings, patches, patches-lib.sh

# Normal builds use the pin. "--from-checkout" builds whatever each repository
# has checked out and records it as the new pin on success: the first build of
# a project, and the build inside update-upstream. No lock and no flag is an
# error, never a quiet switch to the checkout.
FROM_CHECKOUT=no
[[ "${1:-}" == "--from-checkout" ]] && FROM_CHECKOUT=yes
if [[ "${FROM_CHECKOUT}" == yes ]]; then
    reset_source_trees current
else
    [[ -f "${LOCK_FILE}" ]] || die "no ${LOCK_FILE} yet; run compile --from-checkout for the first build"
    reset_source_trees pinned                # source == the recorded commits
fi
trap 'unapply_applied; reset_source_trees current' EXIT   # armed BEFORE applying
apply_tier PREBUILD_PATCHES                  # a failure part-way is still undone
build
install_to_staging
apply_tier POSTBUILD_PATCHES
trap - EXIT
unapply_applied                              # success path: explicit revert
[[ "${FROM_CHECKOUT}" == yes ]] && write_lock   # the pin moves only on success
```

- **`set -euo pipefail` at the top.** Without it a failed compile carries on
  into install and revert, and the pin advances past a build that never
  succeeded.
- **The trap is armed before the first patch is applied.** If patch 7 of 12
  fails, patches 1–6 are taken back off; arming after the apply step would
  leave them on the tree until the next reset.
- **The reset runs first, every time.** It covers the one failure a trap
  cannot: a kill that ends the shell outright (SIGKILL, out of memory, power
  loss). It also runs `git clean -fd`, so files a patch created are removed.
- **`unapply_applied` only reverts what this run applied**, newest first, and
  reports each revert only when the tree actually changed. "Nothing printed" is
  measured, not guessed.

`redownload-source` (delete `source/` and clone again) is the regeneration
lever, safe precisely because all the real work lives in `patches/`.

---

## The pin and the upstream update

`.upstream-lock` is a **pin**, not a note: one line per repository
(`. <commit>` for the main tree, `<path> <commit>` for each nested repository
under `SUBREPO_GLOB`). Every normal build resets to exactly those commits, so
builds are reproducible and "it built yesterday" stays true. Moving the pin is
its own command, `scripts/update-upstream`:

```bash
set -euo pipefail
source "${DIR}/patches/patches.sh"
cp "${LOCK_FILE}" "${DIR}/.upstream-lock.previous"
while IFS= read -r repository; do                       # fetch every repository
    git -C "${repository}" fetch --quiet origin HEAD
    git -C "${repository}" checkout --quiet --detach FETCH_HEAD
done < <(source_repositories)
reset_source_trees current
audit_patches PREBUILD_PATCHES "${DIR}/.upstream-lock.previous"   # may stop here
"${DIR}/scripts/compile" --from-checkout                 # builds, then writes the pin
```

(`verify-patches` resets to the pin while one exists, so run it after the
update's build has moved the pin, or wire it into the build itself.)

The pin moves only at the very end of a successful build; a failure leaves the
previous pin in place, so the next audit always
compares against a version that actually built. A repository that appears
under `SUBREPO_GLOB` but isn't in the lock stops a pinned build with a message
to run the update.

---

## The audit: are these patches still needed?

`audit_patches <tier> <previous-lock>` judges each patch against the pristine
new tree and writes `docs/patches/audit-<commit>.md`, one row per patch:

| Verdict | Meaning |
|---|---|
| OK | anchor present the declared number of times, and upstream didn't touch lines near it |
| REVIEW | upstream changed lines within a few lines of the anchor; the patch may still apply, but what it means there may have changed |
| RETIRE | the anchor is gone and `converged_<ID>` finds upstream now ships the change |
| STALE | the target file is gone, the anchor is gone, or it occurs the wrong number of times |

The changed ranges come from `git diff --unified=0 <old> <new>` hunk headers.
When the source is a shallow clone that lacks the old commit, the audit fetches
it (`git fetch --depth=1 origin <old>`) before diffing, because a diff needs
both ends.

**The report is the worklist.** It is written to a tracked docs directory, one
line per decision, so it survives the next reset (the earlier design inserted
comment tags into the source tree, which dirtied the tree during the build,
shifted the line numbers other patches were anchored to, and was erased by the
next reset before anyone read it). `AUDIT_BLOCKS=yes` makes any non-OK verdict
stop the update; `no` reports and continues — typically `yes` for releases and
`no` while iterating locally.

---

## The generator, the verifier and the registry

- **`scaffold-patch <tier-prefix> <short-name> <path>`** — next free ID, the
  skeleton above, the comment token chosen from the file's extension (an
  unknown extension stops rather than guesses).
- **`verify-patches`** — the gate, wired into the build or CI. For each patch
  in
  the profile, from a clean tree: the probe says "needed"; the `ANCHOR` line
  occurs `MATCHES` times; applying flips the probe; applying again changes
  nothing; unapplying returns the tree to the exact commit; unapplying again
  changes nothing. Then the whole profile is applied together and removed, to
  catch two patches that step on each other. A probe that can only ever say
  "needed" (because it looks in the wrong file) fails the second check, which is
  exactly how a patch silently stops applying in real projects.
- **`gen-registry`** — rebuilds `docs/patches/patch-registry.md` from the
  `TARGET`, `SYMPTOM` and `CHANGE` header fields. Never edited by hand, so it
  can't drift.

---

## The orchestrator (`patches/patches.sh`)

The project file sets the settings `patches-lib.sh` requires (it names them in
its header and refuses to start if any is missing), declares the per-profile
lists, sources every patch file by glob, then sources the library:

```bash
DIR="/path/to/project"
SRC="${DIR}/source"
SUBREPO_GLOB="modules/*"           # nested repositories, or "" for none
LOCK_FILE="${DIR}/.upstream-lock"
AUDIT_DIR="${DIR}/docs/patches"
AUDIT_BLOCKS="yes"
PATCH_HELPER="${DIR}/scripts/replace-exactly.lua"
PATCHES_DIR="${DIR}/patches"
PROFILE="$(cat "${DIR}/.profile")"   # a missing .profile is an error, not "standard"

declare -A PREBUILD_PATCHES=(
    ["minimal"]="P001 P004"
    ["standard"]="P001 P004 P009 P024"
    ["full"]="P001 P002 P003 P004 P009 P024"
)
declare -A POSTBUILD_PATCHES=( ["minimal"]="" ["standard"]="I001" ["full"]="I001 I002" )

for patch_file in "${PATCHES_DIR}"/[PIC][0-9][0-9][0-9]-*.sh; do source "${patch_file}"; done
source "${DIR}/scripts/patches-lib.sh"
```

Adding a file registers it; there is no manifest to keep in sync. Dispatch is by
naming convention, and every lookup demands exactly one match: a profile ID with
no matching function, a patch with no probe, or a profile missing from a tier
array stops the run with the name of what's missing. (A typo in a profile list
used to disable that patch without a word.)

**Profiles** are named selections of patches from the one `patches/` directory.
The "a/b variant" pattern encodes "same setting, different value per profile" as
two sibling patches with non-overlapping profile lists, so exactly one fires and
the old value stays visible as its own file.

---

## De-patching: three mechanisms, each wired to its own trigger

| Mechanism | Wired to | Guarantee |
|---|---|---|
| `unapply_applied` | the EXIT trap and the success path | reverses exactly what this run applied |
| `reset_source_trees` | every build entry and every defensive position | sends each repository to a commit; cannot invent changes |
| `apply-patches --revert` | the operator's control panel | revert by hand; for source patches, triggers an incremental rebuild |

Why defensive cleanup is always a reset and never an unapply: an unapply that
recognises its target by the *patched content* can't tell "we applied this"
from "upstream now ships this", and run defensively on a tree where upstream
converged it reverts clean code into false dirt. Marker-based inverses are
immune (upstream never ships your marker, so there's nothing to match), which is
one more reason markers are the standard. But a reset is simpler still and
correct for every patch style, so it owns every defensive position.

---

## The parts that ship with this skill

`scripts/` in this skill holds working, tested reference versions. Copy them
into
the project's `scripts/`, set `DIR` at the top of each (they refuse to run while
it still reads `__PROJECT_ROOT__`), and adapt:

| File | What it is |
|---|---|
| `replace-exactly.lua` | literal replace with an exact count; `count`, `lines` and `restore` modes |
| `patches-lib.sh` | apply/unapply drivers with probe checks, content-aware revert reporting, pinned reset, lock writing, audit |
| `scaffold-patch` | patch file generator |
| `verify-patches` | the per-patch and whole-profile gate |
| `gen-registry` | registry generator |

The build driver and `update-upstream` are project-specific (what "build" and
"install" mean differs every time), so they appear above as wiring, not files.

For the contribution driver (turning a patch into an upstream pull request) see
`contributing.md`; for non-textual patches, prose patch docs, and the full list
of failures the machine forecloses, see `reference.md`. Both are in this
skill's directory.

---

## Assembling the machine in a new project

- `source/` (or `vendor/`, or the submodule path) in `.gitignore`; commit only
  `patches/`, `scripts/` and `docs/patches/`.
- Copy the shipped scripts, write `patches/patches.sh`.
- Write the build driver with the wiring above; write `update-upstream`.
- First build: `compile --from-checkout` builds whatever is checked out and
  writes the pin at the end; after that, plain `compile` builds the pin.
- Add patches with `scaffold-patch`; nothing enters a profile until
  `verify-patches` passes it.
- Add POST-BUILD or POST-DEPLOY tiers only when the chronology needs them.

---

## Open questions to settle with the owner

- Does each patch need its own issue file, or is the patch header (target,
  symptom, change) its blueprint? The owner's standing instructions decide; if
  they don't say, ask.


ARGUMENTS: add a new B-patch to wow-chat-2026 basic profile:
Player::OCTRegenHPPerSpirit replaced by spirit mana regen
(sqrt(int)*OCTRegenMPPerSpirit*2 per 2s tick)

--------------------------------------------------------------------------------

### Assistant Response 11

> Following the house patch conventions. Reading a recent project patch and the
> per-profile patch list so the new one matches.

> Patch files are picked up automatically. Adding the probe and the basic-list
> entry, then running the project's apply-and-revert test.

> The skip lands the death knight a step in front of Sargobras at the Ebon Hold,
> between its portals to Stormwind and Orgrimmar. Writing the menu option and
> the skip into his script.

> Two failures, both in the test itself: my stand-in kept the first gossip
> handler registered, which is the Lich King's, not Sargobras's. Keying it by
> creature instead.

> All 12 pass. Recording the decision in the death knight issue (718). It
> changes the 2026-09-25 rule that the intro is never skipped into an option.

> Reloading the Lua so the skip is live, then committing the skip and the spirit
> patch.

*[commit] cc4a702a9 in wow-chat-2026 - Death knights may skip their story at Sargobras; spirit heals as fast as it refreshes mana*

> Now the two new issues: Chromie as the keeper of settings (168), and stand-in
> companions for dungeons when you have fewer than four buddies (617t).

The death knight skip and the spirit patch are done and committed; everything
else you asked for this round is written up as issues.

The worldserver is down and the source tree has patches on it, so I assume
you're compiling. The skip is Lua and goes live when the server starts; the
spirit patch is C++ and needs your compile.

## Death knights can skip their story

- **The option:** while a soul is owed, Sargobras in Acherus also offers "I'd
  rather skip the story." After you choose it, each soul in his list reads "Give
  X (level N) to the Lich King, and skip the story."
- **After that trade:** every quest of the Acherus story is taken, completed and
  rewarded in play order, the server's own way. Its spells (Runeforging, the
  Deathcharger, Death Gate), talent points, gear and experience arrive as if
  you'd played it. Then the death knight stands at the Ebon Hold, in front of
  the other Sargobras.
- **Otherwise** nothing changes: the Lich King gives the first quest once the
  soul is paid.
- **The quest list (55 quests)** comes from a new generator that reads the world
  database.
  - The database marks the ten "A Special Surprise" quests only by faction, not
    by race.
  - The generator pins each to its race using the prisoner list in the scene's
    server script, so each death knight gets only its own. Otherwise a skip
    would have given five extra talent points.
- **Test:** 12 offline checks, all passing. Not tried in game yet.
- **Open:** where a quest offers a choice of gear, the skip takes the first
  choice. Should it pick by spec instead?

## Spirit heals as it refreshes (patch B049, issue 167)

Each 2-second health tick out of combat, spirit now heals what it would give
back as mana in 2 seconds while not casting. Warriors, rogues and death knights
have a mana-from-spirit ratio of 0 in the game's tables, so they keep the stock
rate. A level-60 priest with 300 spirit and 300 intellect goes from about
25–30 health a tick to about 69.

The patch applies cleanly in the dry run. That run's round-trip check failed,
but only on files B049 doesn't touch: the tree was already patched when it was
copied, so the check had no clean starting point. I'll rerun it once the tree is
back to clean.

Open:
1. Should warriors, rogues and death knights keep the stock rate?
2. Out of combat only, as stock, or in combat too?

## Issues updated

**Compass formation:**
- Your answers are recorded.
- **Waypoints:** a buddy's waypoint only moves once the buddy reaches it.
- **Send one buddy:** `@party`, `@name` and `@1`/`@2`, several at once. Ours
  will be built beside `rtsc`, deliberately different in five ways (one click
  sends at once, several spread on an arc, routes on the navigation mesh, they
  face your way, a different way back), so you can watch both.
- **Sidesteps:** 20–35% of the circle for melee and 10–20% for ranged, with
  your distance-scaled version as the alternative to compare.
- **Macros:** the server can't hand out macros. The optional addon could create
  the two on first load.
- **Box select:** a new issue, 617s7, for clicking and dragging a box over your
  buddies, waiting on the custom client.

**Tooltips (719):**
- The line shows the calculated number only.
- Cooldowns, durations and costs are included.
- No estimate against the target; abilities only.

## New issues

- **168, Chromie, keeper of settings:** a player's own switches, such as the
  formation on or off, world-fixed directions, and the ring's radius.
  Server-wide settings stay with your config patches.
- **617t, dungeon stand-ins:** with fewer than four buddies, stand-in companions
  fill the dungeon group:
  - exactly one tank and one healer, the rest damage, with no partial-spec
    counting;
  - random white gear for their level in every slot;
  - PvP necklaces, rings and trinkets where no white item exists;
  - gems by class style.

  Meta gems on white weapons is an open question there, as you asked.

## Your questions

- **Haste gems:** yes. The Quick gems (Quick Dawnstone, Quick Lionseye, Quick
  Sun Crystal) are haste rating, and the Reckless ones are haste plus spell
  power. I haven't checked whether basic's gem supply includes them.
- **Examining the LLM conversation system:** yes, I think that's a good idea.
  I'd walk through it from a player's side: what you say or do, what the bot
  hears, what comes back and how long that takes, and where it can fail quietly.
  No code names unless you ask. Say when and I'll start.

## Still open

1. **"5, 5, 25":** default 5 and step 5 are clear. But 25 can't be the minimum
   when the default is 5. Is 25 the new maximum (it was 40)?
2. How often should sidesteps happen?
3. Where does Chromie stand?
4. Should stand-ins be deleted after the dungeon? And does "fewer than 4
   buddy-bots" mean in your roster, or able to come right now?

The new and updated issue files aren't committed; only the death knight skip and
the spirit patch are.

--------------------------------------------------------------------------------

### User Request 12

> - The option: while a soul is owed, Sargobras in Acherus also offers "I'd
> rather skip the story." After you choose it, each soul in his list reads "Give
> X (level N) to the Lich King, and skip the story."

there should also be a second option that says "I changed my mind, I want to
follow the Lich King for a time" which undoes it

> Each 2-second health tick out of combat, spirit now heals what it would give
> back as mana in 2 seconds while not casting. Warriors, rogues and death
> knights have a mana-from-spirit ratio of 0 in the game's tables, so they keep
> the stock rate. A level-60 priest with 300 spirit and
>   300 intellect goes from about 25–30 health a tick to about 69.

Can we have warriors, rogues, and death knights gain the same rate as everyone
else, and make that rate equal to about 1/100th of the amount of stamina (not
hit points) they have? So 100 stamina means 1hp5 per spirit, right? so a tank
with 300 stamina would get 3hp5 per spirit, so 300 spirit means 900hp per 5
seconds, right?

>   2. Out of combat only, as stock, or in combat too?

in combat too. Interrupted by taking damage, and recovers at 50%, just like
spirit does with mana

we'll probably want to add spirit to weapons and armor, but I'm not sure how to
do that in a balanced way. Probably need to replace another attribute or divide
it and use the other parts of the budget as spirit

probably stamina tbh... should keep roughly the same amount of survivability?
Maybe?

> - 168, Chromie, keeper of settings: a player's own switches, such as the
> formation on or off, world-fixed directions, and the ring's radius.
> Server-wide settings stay with your config patches.

she should live in the caverns of time and shattrath city

  1. "5, 5, 25": default 5 and step 5 are clear. But 25 can't be the minimum
     when the default is 5. Is 25 the new maximum (it was 40)?

yes. And 5 is the minimum.

  2. How often should sidesteps happen?

inbetween a spellcast, when they're casting an instant spell

> 4. Should stand-ins be deleted after the dungeon? And does "fewer than 4
> buddy-bots" mean in your roster, or able to come right now?

you can scale them up to the next bracket and have them spawn the next time
someone needs that particular race/class combination.

--------------------------------------------------------------------------------

### Assistant Response 12

> Starting with the spirit patch, since it changes most. One thing your rule
> needs is whether the server already remembers when a player last took damage.
> Checking.

> All four anchors occur exactly once, and the field can go in the player's
> public section. Updating issue 167 first, then rewriting B049.

> Functions are found by patch number, so any name after it works. Writing the
> new patch.

> B049 passes: it applies once even when run twice, both changed files compile,
> and reverting twice returns all three files byte-identical to upstream.
>
> Now the death knight change-of-mind option.

> All 15 checks pass. Recording the change-of-mind option in the death knight
> issue, then your other answers.

> Committing the patch rework, the rename and the death knight option.

*[commit] 6877fb283 in wow-chat-2026 - Spirit heals by stamina for every class, in combat too; death knights can take back asking to skip*

I made all the changes from your answers. The spirit patch and the death knight
option are committed. The other answers are written into their issues but those
files aren't committed. Nothing has run in game yet: the server was down, so the
Lua changes load at its next start, and the spirit patch waits for your compile.

## Spirit heals by stamina (patch B049, issue 167)

- **Every class:** health per 5 seconds = spirit × stamina ÷ 100. Your math is
  right: 300 stamina and 300 spirit gives 900 per 5 seconds, delivered as 360 on
  each 2-second tick. A priest with 150 stamina and 300 spirit gets 450.
- **In combat too.**
- **After taking damage:** half rate for 5 seconds.
- **Unchanged:** sitting still adds a third, and health-regeneration effects
  still multiply it.

To know when someone was last hurt, the player now carries a "last hurt at"
time, set wherever damage lands on a player. That field lives in a header almost
every server file includes, so **your next compile will rebuild most of the
server**.

**Checks:** on clean upstream copies of the three files, applying twice changes
nothing more, both changed files compile with the build's own flags, and
reverting returns all three byte-identical. The patch and issue are renamed to
"spirit heals by stamina"; the first version was never compiled.

## Death knight skip

Once a death knight has asked to skip, Sargobras also offers "I changed my mind,
I want to follow the Lich King for a time." It takes the request back, and the
soul list reads plain again. The offline test now has 15 checks, all passing.

## Answers recorded (uncommitted)

- **Chromie:** lives in the Caverns of Time and Shattrath City.
- **Ring:** default 5, step 5, minimum 5, maximum 25.
- **Sidesteps:** happen between spellcasts, when a buddy is about to cast an
  instant spell.
- **Stand-ins:** kept after the dungeon, raised to the next level bracket with
  gear remade for it, and reused the next time anyone needs that race, class and
  role.

## Open questions

1. **Casters and spirit:** I read "the same rate as everyone else" as the
   stamina rule replacing the mana-based one for casters too, which is how it's
   built. Is that right?
2. **Spirit on gear:** you leaned toward taking its budget from stamina. At what
   exchange rate? Spirit's healing is worth more on a high-stamina character, so
   a straight swap wouldn't keep survivability the same.
3. **Stand-ins:** is a bracket ten levels? And does "fewer than 4 buddy-bots"
   mean in your roster, or able to come right now?
4. **Skip gear:** where a quest offers a choice of gear, the skip takes the
   first choice. Should it pick by spec instead?

The two old patch-file deletions (B016, B019) are still staged from the earlier
retirement. I've left them alone since they aren't mine.

--------------------------------------------------------------------------------

### User Request 13

> - Unchanged: sitting still adds a third, and health-regeneration effects still
> multiply it.

what kind of health regeneration effects?

> To know when someone was last hurt, the player now carries a "last hurt at"
> time, set wherever damage lands on a player. That field lives in a header
> almost every server file includes, so your next compile will rebuild most of
> the server.

how do we handle it for spirit? Can't we use the same system, just abstracted a
little?

Can we add issue files to add Karazhan as an endgame raid? Because it's in
Azeroth...

> - Stand-ins: kept after the dungeon, raised to the next level bracket with
> gear remade for it, and reused the next time anyone needs that race, class and
> role.

including, I might add, for dungeons. But only the low level ones, teehee. Maybe
they can also fill in slots in battlegrounds? Like what if we doubled the number
of buddy-bots that could join a battleground, but the "doubled" amount was the
buddy-bot templates that we had on file? They should be able to be chosen for
*any* higher level bracket btw. But their level doesn't go down.

> 1. Casters and spirit: I read "the same rate as everyone else" as the stamina
> rule replacing the mana-based one for casters too, which is how it's built. Is
> that right?

no the mana regeneration should remain unchanged.

> 2. Spirit on gear: you leaned toward taking its budget from stamina. At what
> exchange rate? Spirit's healing is worth more on a high-stamina character, so
> a straight swap wouldn't keep survivability the same.

How about roughly 1/5th of items have their stamina cost 100% replaced for
spirit. Only if they don't have any spirit already on that item, and also only
if there's at least some stamina on that item. Another 1/5th can have the
stamina amount reduced by half, and made into spirit. We should have roughly the
same amount from each armor category (cloth, leather, mail, plate) normalized
for number of items in that category. Also, we shouldn't touch any gear that
requires a certain class or faction. Also we shouldn't touch PvP gear, there's
enough healing there already... But spirit still takes effect in PvP of course,
just, not from PvP gear.

> 3. Stand-ins: is a bracket ten levels? And does "fewer than 4 buddy-bots" mean
> in your roster, or able to come right now?

yes a bracket is 10 levels. In your roster, unlocked for your level. So between
levels 1 and 30. Alternatively, if the user doesn't have a tank/healer until
level 50 and 60, for levels 30 through 49 the farthest party member that isn't
required can be replaced by a stand-in buddy-bot that is that particular role.

> 4. Skip gear: where a quest offers a choice of gear, the skip takes the first
> choice. Should it pick by spec instead?

No isn't there a way to have a quest appear to everyone in a zone? Can we do
that, and target it specifically to the player in question? Or was that a
cataclysm or later innovation? I thought it was the prepatch maybe... Anyway, if
that's not an option, we can let an NPC in Acherus give a quest that rewards
those items. Maybe even Sargobras. The quest should be auto-completed, so they
can choose.

> The two old patch-file deletions (B016, B019) are still staged from the
> earlier retirement. I've left them alone since they aren't mine.

huh?

--------------------------------------------------------------------------------

### Assistant Response 13

> Several answers and new work here. Gathering facts first, in parallel:
> - which effects multiply health regeneration in this client;
> - how the server tracks the mana "five-second rule";
> - which death knight story quests offer a gear choice;
> - whether any issue already covers Karazhan.

> Applies once. Compiling both changed files with the patched header in front,
> then reverting and comparing.

> Both compile now that the precompiled header is out of the way. That also
> means the earlier `Player.h` version's check wasn't trustworthy; it's replaced
> anyway. Round trip next.

> The round trip is byte-identical. Updating issue 167 to match. Then the
> reward-choice question for the death knight skip: checking which story quests
> offer a choice of gear.

> Karazhan is written (808, four sub-issues). Now spirit on gear, the stand-in
> rules, and the skip's gear choices.

> All valid. Committing the reworked spirit patch and its issue.

*[commit] 0e3007cc7 in wow-chat-2026 - The half-rate healing after damage is now the twin of the mana five-second rule*

I rebuilt the spirit patch on the server's own mana rule, as you asked; it's
committed. Karazhan, spirit on gear, the stand-in rules and the skip's gear
choices are written up as issues, and none of that is built yet.

## Health-regeneration effects

Only a handful of spells in this client change the health-regeneration
percentage:
- **Troll racial, Regeneration:** +10% health regeneration.
- **Death knight Blood Presence.**
- **A few potions and buffs:** Pure Water, the Ahn'Qiraj reward buffs.
- **Mostly monster debuffs that cut regeneration:** Rabies, Cadaver Worms,
  Putrid Bile, Enfeeble, Aura of Suffering.

Sitting adds a third on top. Mana regeneration isn't touched.

## The half rate after damage, built like the mana rule

Your instinct was right. The mana five-second rule works this way:
- every unit (player or creature) keeps "time I last spent mana";
- regeneration asks "was that under 5 seconds ago?".

I rebuilt B049 as that rule's twin: "time I last took damage" and "was that
under 5 seconds ago?", placed right beside the mana versions in the same shape,
set wherever any unit takes damage.

The rebuild cost doesn't go away: any new per-unit time sits in that same widely
included header, just as the mana one does.

**Checks:** applies once, both changed files compile, and reverting is
byte-identical. While checking I found my earlier compile test had been fooled
by the build's precompiled header (a cached copy of the original header). The
check now runs without it.

## Karazhan (issue 808, four sub-issues)

1. Its monsters set for level 60, the same way the Outland dungeons were raised.
2. Its loot placed on the gear ladder.
3. The door opened, since the Master's Key chain is Outland quests, which basic
   disables.
4. Your buddies fill the raid.

Questions:
1. Where on the ladder: between Blackwing Lair and Naxxramas, or above Naxxramas
   as the new top?
2. The door: hand out the key (and how), or open it to anyone at 60?
3. Ten players, or larger?

## Spirit on gear (issue 155k2)

Your rules as written:
- One fifth of eligible items have all their stamina become spirit; another
  fifth, half.
- The fifths are drawn per armour type (cloth, leather, mail, plate), with
  weapons as their own group.
- Only items with some stamina and no spirit; never class-restricted,
  faction-restricted or PvP gear.
- The draw is seeded by item id, so it never changes between rebuilds.

Questions:
1. One stamina for one spirit, or by stat budget? Stamina costs about two thirds
   as much budget as spirit, so a budget-true swap gives fewer spirit points.
2. Are rings, necklaces, cloaks and trinkets in?
3. Does "PvP gear" include world PvP rewards (Halaa, Hellfire's towers)?

## Stand-ins (617t)

Recorded:
- **Who gets them:** a player counts only roster buddies unlocked for their
  level, so everyone below 30 qualifies.
- **Missing roles, 30–59:** a missing tank or healer replaces the farthest
  party member who isn't needed for a role.
- **Reuse:** kept stand-ins work in low-level dungeons, at any higher bracket,
  and never go down a level.
- **Battlegrounds:** double the buddy allowance, the extra half from kept
  stand-ins.

Questions:
1. Up to what level is "low-level" for dungeons?
2. What's the battleground buddy allowance today, the number that doubles?

## Skip gear choices (issue 718)

Only two story quests offer a choice:
- **Tonight We Dine In Havenshire:** one of three cloaks.
- **The Light of Dawn:** the Greatsword or the Greataxe of the Ebon Blade.

So it's your fallback:
- the skip takes back the item the server picked for those two;
- Sargobras at the Ebon Hold then offers two quests of our own that complete as
  soon as you take them, each with the same reward choices.

That's designed, not built yet.

## B016 and B019

Your commit last night ("Two source patches retired") added the retired copies
of those two patches. But the deletion of the original two files
(`B016-playerbots-constructor-reorder.sh` and
`B019-playerbots-unused-parameter.sh`) was left staged in git and never
committed. Until someone commits that deletion, the next ordinary `git commit`
in the project will include it, whatever it's meant to be about. My commits go
around the staging area, so they don't pick it up. Shall I commit just those two
deletions on their own, finishing the retirement?

--------------------------------------------------------------------------------

### User Request 14

Karazhan (issue 808, four sub-issues)

to answer anything about this, I'd need to see the gear stats firefox HTML page
opened. Can you find it for me? It was a claude artifact, and we have a local
copy somewhere. Gotta put it in docs/HTML/

> 1. One stamina for one spirit, or by stat budget? Stamina costs about two
> thirds as much budget as spirit, so a budget-true swap gives fewer spirit
> points.

less about budget, more about combat utility. Healing rate, vs extra opportunity
for healing... Do you help the healer's sustain (spirit) or throughput
(stamina)? If they have to cast powerful, expensive spells to keep up, then
spirit can be better. But if you're taking so much damage they can't get the
slower, more efficient spells off, then stamina might be better. It depends on
the healer, and the character. Trolls usually will want spirit, same for blood
death knights :)

>   2. Are rings, necklaces, cloaks and trinkets in?

no to trinkets. yes to the others.

>   3. Does "PvP gear" include world PvP rewards (Halaa, Hellfire's towers)?

yes, because requires faction, right?

> - Reuse: kept stand-ins work in low-level dungeons, at any higher bracket, and
> never go down a level.

they can also do the same dungeon they just completed. If they do a slightly
higher dungeon, in the same tier, they can be bumped up to that level too. For
each dungeon they complete, they should get a free, powerful blue item for their
spec from the loot table randomly selected of the dungeon they just completed.
But only the visible items, the non PvP ones.

>   1. Up to what level is "low-level" for dungeons?

up to 30, then up to 49 if they lack a class. Actually it's 59 if they get a
tank for example and the player is dps spec, then they could lack a healer until
level 59

>   2. What's the battleground buddy allowance today, the number that doubles?

I think it's 15 for WSG, AB, and 40 for AV? I think EotS is 15. Can we also do
it for arenas? So, 3v3 becomes 6v6 where 3 and 3 are stand-ins.

>   - the skip takes back the item the server picked for those two;
> - Sargobras at the Ebon Hold then offers two quests of our own that complete
> as soon as you take them, each with the same reward choices.

changed my mind, we should just sell those quest items at a vendor in Acherus.
They should be unsellable, nobody wants to handle them because they know that
Acherus wants them back.

> Your commit last night ("Two source patches retired") added the retired copies
> of those two patches. But the deletion of the original two files
> (B016-playerbots-constructor-reorder.sh and
> B019-playerbots-unused-parameter.sh) was left staged in git and never
> committed. Until someone
> commits that deletion, the next ordinary git commit in the project will
> include it, whatever it's meant to be about. My commits go around the staging
> area, so they don't pick it up. Shall I commit just those two deletions on
> their own, finishing the retirement?

oh sure.

--------------------------------------------------------------------------------

### Assistant Response 14

> Starting with the gear page: searching the project and your home folder for a
> saved copy of the "Level-60 Gear Ladder" artifact.

> Found it: `docs/profiles/basic-gear-ladder.html` (the `/home` path is the same
> folder through a link). Checking it's tracked in git and what refers to it,
> then moving it into `docs/HTML/`.

> Moved and indexed. Checking the local copy matches the published artifact,
> then opening it in Firefox.

> Same 46 rows as the published one. Opening it.

> The page should be open in Firefox. Next, finishing the B016/B019 retirement
> through the project's commit tool. A preview first, since that work belongs to
> an earlier session.

*[commit] 858506a76 in wow-chat-2026 - Finish retiring two source patches: their old files go*

> Done; nothing of B016/B019 is left staged. Committing the page move with its
> updated references.

*[commit] 195733577 in wow-chat-2026 - The level-60 gear ladder page moves in with the other HTML pages*

> The new location is committed, but the removal of the old file wasn't picked
> up. My change tracker doesn't see deletions made by git's move command.
> Recording it as mine and committing it.

*[commit] dff1a0f0d in wow-chat-2026 - The gear ladder page's old copy goes (its move's second half)*

> The move is fully committed and git is clean for both paths. Last, writing
> your answers into the issues.

The gear page is at `docs/HTML/basic-gear-ladder.html` and should now be open in
Firefox. It's the same 46 rows as the published artifact, and the table of
contents and the two notes that pointed at its old place now point at the new
one. The B016/B019 retirement is finished and committed. Your other answers are
written into their issues, uncommitted.

**Spirit on gear (155k2):**
- **No fixed exchange rate:** the item offers the choice between helping the
  healer's sustain (spirit) and throughput (stamina), and which is better
  depends on the wearer. Your troll and blood death knight note is in.
- **Which items:** rings, necklaces and cloaks are in; trinkets are out.
- **World PvP rewards are out:** yes, they need a faction's standing, so the "no
  faction-restricted items" rule already excludes them.
- **Open:** with no budget rate, is it one stamina becomes one spirit?

**Stand-ins (617t):**
- **Dungeons:** up to 30; up to 49 while a tank or healer is missing; up to 59
  if you have a tank, play damage yourself, and still lack a healer.
- **Repeat runs:** a stand-in may repeat the same dungeon, or move up to a
  higher one in the same tier and be raised to its level.
- **Reward:** each dungeon completed gives it one random blue from that
  dungeon's loot, for its spec, visible items only, no PvP gear.
- **Battlegrounds:** each side doubles, the added half stand-ins. A correction:
  Warsong Gulch is 10 a side in this version, not 15. Arathi Basin, Eye of the
  Storm and Strand of the Ancients are 15; Alterac Valley and Isle of Conquest
  are 40.
- **Arenas:** 2v2 becomes 4v4, 3v3 becomes 6v6, 5v5 becomes 10v10.
- **Open:** bigger sides mean raising the server's own size limits. Should real
  players also be able to queue into the bigger sizes, or should only stand-ins
  fill the extra places?

**Death knight skip gear (718):** a vendor in Acherus sells the five choice
items: the three cloaks from Havenshire and the two Ebon Blade two-handers. None
of them can be sold to any vendor, and the skip keeps whichever one the server
picks. **Open:** which Acherus vendor, and at what price? Not built yet.

**Karazhan:** the open questions are waiting until you've looked at the page:
1. Where on the ladder does it go?
2. How do players get through the door, now that the Master's Key chain is
   disabled?
3. Ten players, or larger?

--------------------------------------------------------------------------------

### User Request 15

can we have kharazhan's item level rewards span from the level 60 dungeon blues
to the beginning 20% of Molten Core's item level distribution? Also, can we
range the required level from 58 to 60 at the end for the final bosses, because
they're that hard? It's an azeroth dungeon. It should be treated as such, but,
it's a raid. Beginner raid. Ideally, we'd also have Zul'Aman and Zul'Gurub as
another raid tier, maybe in the early 50s? question... Is the various Maraudon
and Dire Maul dungeons connected or on the same map in any way? Can we create a
connection between them? I'm thinking we could make the entire dungeon
collection into a raid, where if you're going faster (proportionally) than the
other side, you get a tougher enemy to fight until you slow down, and the other
starts keeping pace with you. Then you can speed up until you face something
that you can't. Then, you are expected to swap characters, which you can do by
simply moving them into a different raid group. You teleport to the centroid
between your allies, on the closest spot on the pathable grid that is also
within an un-obstructed 10-yard straight-line path.

Note, it should never hurt you when your allies are doing well. It should
challenge them more, as they escalate their fights. So, emphasize the level
difference over spans.

> - Open: bigger sides mean raising the server's own size limits. Should real
> players also be able to queue into the bigger sizes, or should only stand-ins
> fill the extra places?

only stand-ins. Also, we should make it so that anyone who is in a duel can
fight and deal damage to anyone else who is in a duel near them. Also there
should be at least 15 stand-ins always fighting at all times in front of
Orgrimmar and Stormwind - they are always on alert, practicing and recovering
and staying fit and strong. They are from a variety of different levels, and
every 15 duels +/- 1 they go to the side and sit down next to a fire for a
while. Also, duels end at 20% health on this server. Basically everytime they
lose a fight, they go sit on the side until they recover. Also, we should make
campfires stack, so they can boost each other's regeneration if they sit near
each other.

also can we ensure that buddy-bots always always get teleported correctly when
the player-partner casts hearthstone? Even if their cast didn't complete, or if
they were interrupted by combat or dying... we still teleport them manually with
lua

  1. Where on the ladder does it go?

between BRD and MC's first 20% of item levels okay maybe not BRD it's item
levels 58 and 47 for blues and epics respectively. Up until... let's say... 63?
With a gap from 60. if there's any karazhan blues or w/e they can be 47 to 63
for epics, and 58 to... some other number for blues.

> 2. How do players get through the door, now that the Master's Key chain is
> disabled?

can we add a flight path from a high vantage point that can see down onto it?
The flight path creature can glide down, then walk back up with an escort
waiting down below.

  3. Ten players, or larger?

I forgot the default size, is it 10? How parallelizable is this dungeon?

--------------------------------------------------------------------------------

### Assistant Response 15

> A long list. Doing the one you need working first (buddies always following a
> hearthstone) and checking facts in parallel. The rest becomes issues.
>
> Facts first: Molten Core's item-level spread, Karazhan's size and its bosses'
> item levels, and whether Maraudon and Dire Maul share a map.

> Writing its offline test: a stand-in partner with three buddies (one dead, one
> mid-cast), plus a buddy hearthing on its own.

> All 10 checks pass. Recording it in the buddy-adventuring issue, then
> committing. The Lua takes effect at the server's next start (or a reload, if
> it's up).

*[commit] 76e32102e in wow-chat-2026 - Buddies always follow their partner's hearthstone*

Buddies now always follow your hearthstone; that's committed. Everything else
this round is written up as issues, nothing built. The server is still down, so
the hearthstone follow goes live at its next start.

## Buddies follow your hearthstone, always

When your hearthstone (or a shaman's Astral Recall) finishes, 1.5 seconds later
each of your buddies in the world has its own casting stopped, then is moved
onto an arc around where you arrived, facing you. That happens alive or dead,
mid-fight or not, whatever happened to its own hearthstone.

A buddy that finishes its own hearthstone, which still points at its starting
valley, is sent to your bind spot instead.

The offline test passes 10 of 10. It's recorded in the buddy-adventuring issue.

## Your questions

- **Are Maraudon and Dire Maul connected?** No. They're separate instance maps:
  Maraudon's entrance is in Desolace, Dire Maul's in Feralas, two zones that
  border each other. They can be joined with portals that teleport straight from
  one instance to the other. That's the first step of issue 810.
- **Karazhan's default size** is ten players.
- **How parallelizable is Karazhan?** From memory of the stock raid, so it needs
  checking against its door scripts: after Attumen and Moroes, several bosses
  can be done in any order (the Maiden, the Opera, the Curator's path, the
  Servants' Quarters). Shade of Aran and Netherspite open after the Curator, and
  Chess, Prince and Nightbane come later. So two groups could split for the
  middle part.
- **Molten Core's spread**, for your Karazhan numbers: its drops apart from the
  Tier 1 set (item level 66) run from item level 58 to 71. The bottom 20% are at
  61 or below; the median is 65.

## Written up as issues (uncommitted)

**808, Karazhan:**
- **The tier:** a beginner raid with the level-60 dungeon blues at the bottom.
- **Loot:** epics item level 47–63, blues from 58 up.
- **Required level:** 58, rising to 60 for the last bosses' loot.
- **The door:** a flight from a vantage point in Deadwind Pass that glides down
  to the door, the mount walking back up, and an escort waiting below.

**809, Zul'Aman and Zul'Gurub:** a raid tier for the early fifties.

**810, Maraudon and Dire Maul as one raid:**
- **Pacing:** each side's progress is measured as a share of its own wing. A
  side that pulls ahead meets higher-level enemies, in steps, until the others
  catch up, and allies doing well never makes your side harder.
- **Swapping:** moving a character to another raid group puts it at the middle
  point of that group's members, on the nearest walkable spot with a clear
  straight line of at most 10 yards to that point.

**169, duels:**
- Anyone in a duel can hit anyone else in a duel nearby.
- Duels end at 20% health.

**170, sparring grounds at Orgrimmar and Stormwind:**
- At least 15 stand-ins of mixed levels duel at all times.
- Every 15 duels (give or take one), and after every loss, a stand-in sits by a
  fire to recover.
- Campfires stack, so stand-ins resting side by side boost each other's
  regeneration.

**617t, stand-ins:** the doubled battleground and arena places are filled by
stand-ins only. Players queue to the normal sizes.

## Open questions

1. **Karazhan items:** what does "with a gap from 60" mean (no items between
   which level and 60?), and where do Karazhan's blues top out?
2. **Karazhan size:** ten, or larger now that two groups can split inside?
3. **Zul'Aman and Zul'Gurub:** which levels exactly (50–54?), which item
   levels, and their stock sizes (Zul'Gurub 20, Zul'Aman 10)?
4. **The joined raid:** one side per wing (five or six), or two (Maraudon and
   Dire Maul)? How big is a level step, and over how much progress?
5. **Duels:** how near is "near"? Can you heal someone in another nearby duel?
   Does a duel loser at 20% stay in the shared fight?
6. **Campfires:** how much does one fire add to regeneration, is there a cap on
   stacking, and does stacking apply to all campfires or only the sparring
   grounds'?

Still open from before: the stamina-to-spirit point rate, and which Acherus
vendor sells the death knight story items, at what price.

--------------------------------------------------------------------------------

### User Request 16

- Molten Core's spread, for your Karazhan numbers: its drops apart from the Tier
  1 set (item level 66) run from item level 58 to 71. The bottom 20% are at 61
  or below; the median is 65.

yes and the NPCs should be level scaled such that the earliest ones are level
53ish I think? Same approximate level as BRD. The final boss and the last 20% of
bosses are level 60 only.

> - Swapping: moving a character to another raid group puts it at the middle
> point of that group's members, on the nearest walkable spot with a clear
> straight line of at most 10 yards to that point.

at most 10 yards to the point of the nearest group member. So they are
teleported 10 yards away, at the closest spot to their spot.

> - Every 15 duels (give or take one), and after every loss, a stand-in sits by
> a fire to recover.

every fifteenish duels they tap out with another person who came to watch on at
their 14ish'th duel. They'd stand by the side, and when the 15th duel came
around they'd replace them in the fight. which, by the way, is a better way to
guard places.

>   2. Karazhan size: ten, or larger now that two groups can split inside?

if 10 is the default then 10

> 4. The joined raid: one side per wing (five or six), or two (Maraudon and Dire
> Maul)? How big is a level step, and over how much progress?

each tier is joined to the same one. So Maraudon has many parallel paths through
it, right? Same for Dire Maul. Also Scarlet Monastery. You could argue Razorfen
Kraul and Downs. Possibly others... WC is parallelizable, go left at the T or
right, team up for the murloc fight... But SFK is not parallelizable, there's
only one route through. Can you ask me about various dungeons and I'll provide a
final ruling?

> 5. Duels: how near is "near"? Can you heal someone in another nearby duel?
> Does a duel loser at 20% stay in the shared fight?

dueling should just be consenting to free-for-all-ing. If you drop to 20% health
you can't be targeted or ask for duels or be asked until you hit 100% health
again.

> 6. Campfires: how much does one fire add to regeneration, is there a cap on
> stacking, and does stacking apply to all campfires or only the sparring
> grounds'?

1 campfire gives 1 spirit per campfire within 20 yards. If any are overlapping
each other, only one of them counts, whichever one is closer to the centroid of
the other campfires. (second pass calculation)

> Still open from before: the stamina-to-spirit point rate, and which Acherus
> vendor sells the death knight story items, at what price.

I think it's stamina * spirit / 100? The acherus vendor... Pick someone noble
for the capes, and someone vicious for the weapons.

--------------------------------------------------------------------------------

### Assistant Response 16

> Recording your answers. Choosing the two Acherus vendors means seeing who's
> there, so first: listing the people standing in Acherus.

All your answers are written into their issues. None of these files are
committed yet, and nothing new is built.

**Recorded:**
- **Karazhan (808, 808a):**
  - **Size:** ten players.
  - **Monster levels:** the first bosses and their halls about level 53
    (Blackrock Depths' level), rising boss by boss. The last fifth of the
    bosses, including the final boss, are level 60.
- **Swapping groups (810):** a moved character lands within 10 yards of the new
  group's member nearest that group's middle, on the walkable spot closest to
  them with a clear line.
- **Duels (169):** a duel is consent to a free-for-all, so you can hit and heal
  any other duellist. At 20% health you're out: untargetable, and you can't ask
  for or be asked to a duel until your health is full.
- **Sparring (170):** around a fighter's 14th duel a fresh stand-in comes to
  watch from the side; at the 15th it takes the fighter's place and the fighter
  rests by a fire. A loss also sends a fighter to rest. The same relay is noted
  as the model for guard posts.
- **Campfires (170):** +1 spirit for each campfire within 20 yards. Of fires
  that overlap, only the one nearest the middle of the others counts.
- **Death knight vendors (718):**
  - **Cloaks:** Lady Alistra, a noble death knight trainer in the upper hall.
  - **Weapons:** Instructor Razuvious, the drillmaster beside the Lich King.

  Neither is a vendor today, so each gains a shop.

**Dungeon rulings for the joined raids (810).** You said Wailing Caverns,
Scarlet Monastery, Maraudon and Dire Maul are parallel, and Shadowfang Keep
isn't. The rest, from my memory of the stock maps (worth checking), for your
final ruling:

| Dungeon | Paths through it |
|---|---|
| Ragefire Chasm | one route |
| The Deadmines | one route |
| Blackfathom Deeps | mostly one route, a few side rooms |
| The Stockade | two halls off one corridor |
| Gnomeregan | front and back entrances meet partway |
| Razorfen Kraul | branching tunnels |
| Razorfen Downs | mostly one route |
| Uldaman | two entrances, then mostly one route |
| Zul'Farrak | an open courtyard, bosses in any order |
| Sunken Temple | a ring of rooms, some in any order |
| Blackrock Depths | many branches, much in any order |
| Lower Blackrock Spire | branches, a few in any order |
| Upper Blackrock Spire | one route |
| Scholomance | a ring of rooms, several in any order |
| Stratholme | two halves: the living side and the undead side |

**Open questions:**
1. **Spirit on gear:** "stamina × spirit / 100" is the healing rule from the
   spirit patch, which is what spirit does once it's on an item. The open
   question is how many spirit points an item gets for the stamina it gives up.
   I propose one for one, so 20 stamina becomes 20 spirit.
2. **Campfires:**
   - Does "overlapping" mean fires whose flames touch (within a couple of
     yards), or fires whose 20-yard reaches overlap? The second would let only
     one fire count in any cluster, which would cancel the stacking.
   - Does stacking apply to all campfires, or only the sparring grounds'?
3. **Death knight vendors:** what price for the five story items?

--------------------------------------------------------------------------------

### User Request 17

- Monster levels: the first bosses and their halls about level 53 (Blackrock
  Depths' level), rising boss by boss. The last fifth of the bosses, including
  the final boss, are level 60.

make sure the item levels of the dropped items are adjusted accordingly

> - Duels (169): a duel is consent to a free-for-all, so you can hit and heal
> any other duellist. At 20% health you're out: untargetable, and you can't ask
> for or be asked to a duel until your health is full.

healers should be able to heal anyone in a duel without being engaged in a duel.
They will run out of mana eventually. Also can we remove the health restorative
effects of food and water when near a duel? It's not good to fight where you
eat. So characters have to either walk far away (back into the city, to a cafe
or something for food) or they can just sit by the fire with their allies at the
duel place. Either way, there must be the correct number of combatants at all
time, so if they want to leave another ally will walk to the duel area, until
that time they wait, when the ally arrives they can swap places and go wander
about.

> - Sparring (170): around a fighter's 14th duel a fresh stand-in comes to watch
> from the side; at the 15th it takes the fighter's place and the fighter rests
> by a fire. A loss also sends a fighter to rest. The same relay is noted as the
> model for guard posts.

instead of resting by the fire, the stand-in should walk to an inn or resting
spot (buddy-bot beds issue perhaps?) and goes to sleep. They'll despawn after
30-45 seconds. Stored in the database for the next task they're job is. That
way, we maintain the same amount.

> - Campfires (170): +1 spirit for each campfire within 20 yards. Of fires that
> overlap, only the one nearest the middle of the others counts.

no, +1 spirit per campfire for each campfire within 20 yards. So 5 campfires
gives 5 spirit per campfire, for a total of 25 spirit. They can't overlap so the
max is probably 3*20 squared or something? Sorry math is hard



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Ragefire Chasm │ one route │
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ The Deadmines │ one route │
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Blackfathom Deeps │ mostly one route, a few side rooms │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


these are single-threaded



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ The Stockade │ two halls off one corridor │
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Gnomeregan │ front and back entrances meet partway │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


these are parallel



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Razorfen Kraul │ branching tunnels │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


near the last boss, by the tuber quest, there's a labyrinth that has hardly any
monsters in it. We should make an issue file for populating that area, then we
can have two groups in there I think.



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Uldaman │ two entrances, then mostly one route │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


Uldaman can be a raid.



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Sunken Temple │ a ring of rooms, some in any order │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


Sunken Temple can be a raid.



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Blackrock Depths │ many branches, much in any order │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


definitely



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Lower Blackrock Spire │ branches, a few in any order │
  ├───────────────────────┼─────────────────────────────────────────────────┤
</pasted_content id="7f61">


yup



<pasted_content id="7f61">
  ├───────────────────────┼─────────────────────────────────────────────────┤
  │ Stratholme │ two halves: the living side and the undead side │
  └───────────────────────┴─────────────────────────────────────────────────┘
</pasted_content id="7f61">


yesssss

> 1. Spirit on gear: "stamina × spirit / 100" is the healing rule from the
> spirit patch, which is what spirit does once it's on an item. The open
> question is how many spirit points an item gets for the stamina it gives up. I
> propose one for one, so 20 stamina becomes 20 spirit.

1x stamina gives 10hp, and 1x spirit gives 0.1hp per stamina. So really, stamina
gives 0.1hp per spirit too. Is that right? Per 5 seconds? that means it takes
100 seconds to recover 1 stamina worth of hit points... That seems, kinda bad
actually. Wait hang on my math is incomplete, can you help? Math is hard :(

>      - Does "overlapping" mean fires whose flames touch (within a couple of yards), or fires whose 20-yard reaches overlap? The second would let only one fire count in any cluster, which would cancel the stacking.

within a couple of yards

>      - Does stacking apply to all campfires, or only the sparring grounds'?

all campfires

>   3. Death knight vendors: what price for the five story items?

1g each, they sell for no gold. Or 1c if we need to.

--------------------------------------------------------------------------------

### Assistant Response 17

> Your spirit math first, since you asked for help; then everything gets
> recorded.
>
> **Spirit healing.** The rule is health per 5 seconds = spirit × stamina ÷
> 100, and each point of stamina is worth about 10 health. Stamina sets both the
> size of your health pool and how fast spirit refills it, so it cancels out:
> - Every 5 seconds you regain spirit ÷ 10 percent of your maximum health.
> - So 100 spirit refills 10% every 5 seconds, which is empty to full in 50
>   seconds. 300 spirit is 30% every 5 seconds, about 17 seconds to full.
> - At half rate after taking damage, double those times.
>
> Your "100 seconds to recover 1 stamina worth" is right for a character with
> only 1 spirit. At real amounts of spirit it's much faster.
>
> **What the 1:1 gear swap trades** (example: 20 stamina becomes 20 spirit, on a
> character with 5,000 health):
> - You lose 200 health from your pool, once.
> - You gain 2% of 5,000 = 100 health every 5 seconds.
> - Spirit catches up with the lost stamina after about 10 seconds of healing at
>   full rate, or 20 seconds at half rate.
>
> So in fights longer than that, spirit gives more total health; for burst,
> stamina wins. That's your sustain-versus-throughput trade.
>
> Recording everything now.

Everything is recorded in the issues, plus one new issue (811). These files are
uncommitted, as before.

**Recorded:**
- **Karazhan loot (808b):** each boss's loot follows that boss's level, so its
  item level and required level rise with it, from about level 53 to 60.
- **Duels (169):**
  - Anyone can heal a duellist without joining the fight.
  - Food and drink restore nothing near a duel, so fighters either walk back to
    the city to eat or sit by the fire at the duel ground.
  - A fighter who wants to leave waits until a replacement walks up, then swaps
    out.
- **Sparring (170):** a fighter who's swapped out walks to an inn or a bed, lies
  down, and leaves the world 30–45 seconds later, kept in the database for its
  next job.
- **Campfires (170):** stacking applies to all campfires. Fires within a couple
  of yards of each other count as one.
- **Death knight items (718):** 1 gold each, and they sell for nothing.
- **Dungeon rulings (810):**
  - **Single route:** Ragefire Chasm, the Deadmines, Blackfathom Deeps.
  - **Parallel:** the Stockade, Gnomeregan, Blackrock Depths, Lower Blackrock
    Spire, Stratholme.
  - **Raids:** Uldaman and the Sunken Temple.
  - **Razorfen Kraul:** parallel once its labyrinth is populated, which is the
    new issue 811: it fills the near-empty maze by the tuber quest with packs of
    quilboar and their beasts at the nearby halls' levels, so a second group has
    a path.
- **Spirit on gear (155k2):** the math above is written into the issue.

**Campfire stacking has no ceiling as stated.** With n campfires within 20
yards, each gives n spirit, so n × n in total (5 fires make 25). A 20-yard
circle holds about 1,257 square yards, and fires kept 2 yards apart each need
about 3.5 of those. So about 360 fires fit, which would give about 130,000
spirit; at 3 yards apart, about 160 fires and 26,000 spirit. Without a cap,
enough fires make spirit unlimited.

**Open questions:**
1. **Campfire cap:** at most how many fires should count? Five gives 25 spirit,
   ten gives 100.
2. **Food near duels:** how close to a duel does food stop working?
3. **Spirit on gear:** with the math in hand, is one spirit for one stamina
   right?
4. **Dungeons still without a ruling:** Zul'Farrak (an open courtyard, bosses in
   any order), Razorfen Downs (mostly one route), Upper Blackrock Spire (one
   route) and Scholomance (a ring of rooms, several in any order).

--------------------------------------------------------------------------------

### User Request 18

for the stockade dungeon, can we spawn the players on the inside, one in each
wing, depending on which raid group they're in? They can meet up in the middle.
There should be twice as many enemies in the middle sections.

>     - Food and drink restore nothing near a duel, so fighters either walk back to the city to eat or sit by the fire at the duel ground.

also monsters deal 2x damage to dueling characters

what's the difference between "parallel" and "raids"? characters are split
between entrances, or perhaps in a sister dungeon progressing at the same time,
with a shared party chat and coordination... and the ability to easily switch
groups, so you can put the correct attackers and defenders to the right tasks.

>     - Razorfen Kraul: parallel once its labyrinth is populated, which is the new issue 811: it fills the near-empty maze by the tuber quest with packs of quilboar and their beasts at the nearby halls' levels, so a second group has a path.

we will need bosses too, so we should bring level adjusted rare elite quilboards
from everywhere in the map and have them wander through the labyrinth. The
patrols and the rares should try and spread themselves throughout the maze, with
both of them sharing separate pathing maps. Make sure their loot is level
adjusted too, we will probably have to make duplicates or something because we
want them to also spawn in the world. It's just... they're wandering through.
There should be about as many as there are bosses on the previous path.
Remember, if the other paths are say... 30% of the way through the dungeon and
you're 50% of the way through, then there's a 20% discrepancy. This means the
bosses and monsters maybe have 20% more stats? Or levels? I dunno. But they
should be harder. The labyrinth is very dense, which means that it will have
less walking, meaning, parties will clear it faster. So it's gotta be tougher
too, and depending on how fast... it might seem to speed up partway through. Or
slow down if you're doing great at your fights. But it only adjusts for monsters
that are not in combat.

> Campfire stacking has no ceiling as stated. With n campfires within 20 yards,
> each gives n spirit, so n × n in total (5 fires make 25). A 20-yard circle
> holds about 1,257 square yards, and fires kept 2 yards apart each need about
> 3.5 of those. So about 360 fires fit, which would
> give about 130,000 spirit; at 3 yards apart, about 160 fires and 26,000
> spirit. Without a cap, enough fires make spirit unlimited.

haha absurd. How about 3 yards apart?

>   2. Food near duels: how close to a duel does food stop working?

100 yards

> 3. Spirit on gear: with the math in hand, is one spirit for one stamina right?

I dunno probably

> 4. Dungeons still without a ruling: Zul'Farrak (an open courtyard, bosses in
> any order), Razorfen Downs (mostly one route), Upper Blackrock Spire (one
> route) and Scholomance (a ring of rooms, several in any order).

Zul'Farrak is a loop, so, let's say it's serial

Razorfen Downs is 1 route, but if you wanted we could double the player count
and double the number of creature monsters.

for Scholomance let's say it's a dungeon for now. For Blackrock Spire, let's say
UBRS and LBRS are combined, and each one is level adjusted such that LBRS has
the same level range as UBRS, which is the range from their current minimum to
their current maximum. And each one will be affected by the buff/debuff system.

How many players in BRD? Can you give me estimates for number of looted items
per route? We can go through them one-by-one or two-by-two if you please in a
table that lists each boss, and how many items it drops. Also, which path it's
on, with an arbitrary label. Then we will tune how many items they drop. Also
can you add how many items they are able to drop? To assess variety - give the
more varied ones more item drops comparatively.

--------------------------------------------------------------------------------

### Assistant Response 18

> Answering the "parallel versus raid" question, then BRD, which needs real loot
> data. Pulling Blackrock Depths' bosses and their loot from the world database
> now.

> That filter caught only some. Querying the full boss list by name instead.

> Some numbers look wrong: the Emperor at 0.0 drops per kill, most bosses at
> 0.3. Looking at their raw loot rows before trusting any of it.

All your answers are recorded. None of these issue files are committed yet.

## "Parallel" versus "raid"

There's really one thing, which I'll call **the joined run**: a raid group whose
parties progress at the same time, sharing raid chat, with any character movable
between parties and the pacing between them. The words in the ruling table only
describe how a dungeon holds a joined run:
- **Parallel:** the dungeon's own map has separate paths, one party each.
- **Single:** one route, so it joins a run only beside a sister dungeon, or by
  doubling players and monsters (as you said for Razorfen Downs).
- **"A raid":** your word for Uldaman and the Sunken Temple; it means the same
  joined run.

That definition is now written into issue 810.

## Recorded

- **The Stockade:** each party appears inside at the far end of its own wing, by
  party number. You meet in the middle, where the enemies are doubled.
- **Rulings:**
  - **Zul'Farrak:** single (a loop).
  - **Razorfen Downs:** single, with the doubling option.
  - **Scholomance:** a dungeon for now.
  - **Blackrock Spire:** Upper and Lower joined into one run. Each half spans
    the lowest-to-highest level of the pair, and both are under the pacing.
- **The labyrinth (811):**
  - **Bosses:** copies of the world's rare elite quilboar, with their levels and
    their loot's levels set for the labyrinth. The originals stay in the world.
    There are as many as the other path has bosses.
  - **Movement:** patrols and rares walk the maze on two separate distance maps,
    spreading away from their own kind.
  - **Harder:** it's tougher than the other path because it's dense, with little
    walking between fights.
  - **Pacing:** the step changes only monsters not yet in combat.
- **Duels (169):** monsters do double damage to duellists, and food stops
  working within 100 yards of a duel.
- **Fires (170):** fires within 3 yards of each other count as one.
- **Spirit on gear (155k2):** one stamina becomes one spirit, as the working
  rule.

## Blackrock Depths

Five players, as in any stock five-player dungeon. Below is the loot per boss,
from a scratch count of the world database's loot tables:
- **Drops per kill** counts expected green-or-better items per kill. Quest
  items, grey junk and trade goods aren't counted.
- **Own items** is how many different green-or-better items the boss can drop
  from its own lists.
- **Shared pool** is the zone's random world drops some bosses also roll on.
- **The path labels are arbitrary**, and I placed bosses on paths from memory of
  the map, so correct anything that's wrong.

| Path | Boss | Level | Drops per kill | Own items | Shared pool |
|---|---|---|---|---|---|
| A: prison | High Interrogator Gerstahn | 52 | 1.0 | 4 | – |
| A: prison | Lord Roccor | 51 | 1.3 | 6 | – |
| A: prison | Houndmaster Grebmar | 52 | 1.0 | 4 | – |
| B: Ring of Law (one of six per run) | Anub'shiah | 52 | 1.1 | 4 | – |
| B: Ring of Law | Eviscerator | 52 | 1.0 | 4 | – |
| B: Ring of Law | Gorosh the Dervish | 52 | 1.0 | 4 | – |
| B: Ring of Law | Grizzle | 52 | 2.0 | 5 | – |
| B: Ring of Law | Hedrum the Creeper | 52 | 1.0 | 4 | – |
| B: Ring of Law | Ok'thor the Breaker | 52 | 1.0 | 4 | – |
| C: tavern | Hurley Blackbreath | 55 | 1.0 | 4 | – |
| C: tavern | Phalanx | 55 | 1.0 | 4 | – |
| C: tavern | Plugger Spazzring | 55 | 0.5 | 3 | – |
| C: tavern | Ribbly Screwspigot | 53 | 1.0 | 4 | – |
| D: forges | Bael'Gar | 52 | 1.0 | 4 | – |
| D: forges | Fineous Darkvire | 53 | 1.0 | 4 | – |
| D: forges | Lord Incendius | 53 | 1.0 | 5 | – |
| D: forges | General Angerforge | 55 | 1.0 | 5 | – |
| D: forges | Golem Lord Argelmach | 55 | 1.6 | 5 | – |
| E: vault & shrine | Warder Stilgiss | 54 | 1.0 | about 340 with the pool | 52 |
| E: vault & shrine | Verek | 53 | 0.3 | about 340 with the pool | 52 |
| E: vault & shrine | Pyromancer Loregrain (rare) | 52 | 1.2 | about 350 with the pool | 59 |
| E: vault & shrine | Watchman Doomgrip | 54 | 0.4 | 65 | 742 |
| F: inner city | Ambassador Flamelash | 55 | 1.0 | 5 | – |
| F: inner city | The Seven (their chest) | 55–56 | 2.0 | 8 | – |
| F: inner city | Magmus | 56 | 1.0 | 5 | – |
| G: throne | Princess Moira Bronzebeard | 55 | 1.0 | 4 | – |
| G: throne | Emperor Dagran Thaurissan | 56 | 2.0 | 11 | – |
| (rare, wanders) | Panzor the Invincible | 56 | 1.0 | about 300 with the pool | 104 |

What the table shows:
- Most bosses drop one item from four or five possibilities.
- The **Emperor** (2 per kill, 11 possible) and **The Seven's chest** (2, 8
  possible) have the most variety.
- **Grizzle** and **Golem Lord Argelmach** give more per kill than their peers.
- **Plugger Spazzring** gives the least (0.5 per kill, 3 possible).

## Open questions

1. **Campfire cap:** 3-yard spacing still allows about 160 fires in a 20-yard
   circle, so n × n could reach about 26,000 spirit. At most how many fires
   should count?
2. **Labyrinth pacing:** if a path is 20 points ahead, how much harder do its
   monsters get: more levels (+2?) or more stats (+20%)?
3. **Labyrinth packs:** how many packs?
4. **Blackrock Depths loot:** which bosses should drop more or less? Your rule
   was to give the more varied ones more drops. Do the path labels look right?

--------------------------------------------------------------------------------

### User Request 19

once we've successfully planned the item levels for each dungeon and raid, can
you re-build the gear level chart? We can expand it out horizontally if you'd
like.

can you update the buddy-bot stat profile preferences according to the relative
value compared to stamina of the spirit update? It should update dynamically as
their gear shifts. They should try and keep them in balance.

for BRD, can we have one path be prison and ring of law, then they fight their
way to the tavern and everyone inside is hostile. The second party starts in the
tavern, and everyone is friendly. Second party goes through forges and vaults.
Inner city and throne are a third route, for a total of 15 players.

We need to make sure there's the same amount of item drops on each route. Number
of items dropped. We should drop more items from characters who have large loot
pools. Also, rare elites should always spawn, and they should be included in
these calculations.

> 1. Campfire cap: 3-yard spacing still allows about 160 fires in a 20-yard
> circle, so n × n could reach about 26,000 spirit. At most how many fires
> should count?

how about 4 and 5 yards?

> 2. Labyrinth pacing: if a path is 20 points ahead, how much harder do its
> monsters get: more levels (+2?) or more stats (+20%)?

not sure about the scaling on it...

  3. Labyrinth packs: how many packs?

enough so that it's hard to pull. they redistribute while you're in the
labyrinth...

Each BRD boss should give a minimum of 1.0 items per fight

--------------------------------------------------------------------------------

### Assistant Response 19

> Checking where buddies score gear today, so the spirit-against-stamina value
> can go in the right place.

> Adding the buddy module's answer and its declaration.

> Now the bot-module patch, B050.

> B050 is written and registered. Testing it on a clean copy of the scorer:
> apply, compile, revert, compare. Then compiling the buddy module's file.

> B050 passes: it applies once, both changed files compile with the build's
> flags, and the revert is byte-identical. Committing it with the buddy-module
> change and the issue.

*[commit] 018e58312 in wow-chat-2026 - Buddies weigh spirit by what it heals, so they keep spirit and stamina in balance*

> The issue file was left out because my last edit to it was made by a script.
> Claiming and committing it.

*[commit] 22d3525ae in wow-chat-2026 - The issue describing how buddies weigh spirit against stamina*

The buddy spirit weighting is done and committed; the rest of this round is
planning recorded in the issues.

**Buddies weigh spirit by what it heals (patch B050, issue 617j3).** Whenever
the bot module scores an item for a buddy, it asks the buddy module what one
spirit is worth in stamina for that buddy right now. The comparison is health
over a 30-second fight:
- one more spirit heals stamina ÷ 100 every 5 seconds;
- one more stamina holds 10 health and also adds to spirit's healing.

Stamina's weight times that worth is added on top of spirit's normal weight,
which mana users already get. It's worked out from the buddy's current stats
every time, so the more stamina it has the more spirit is worth, and vice versa;
its choices drift toward balance. With 300 stamina and 100 spirit, one spirit is
worth about 1.1 stamina; with 300 of each, about 0.64. Players and other bots
are unchanged.

It applies once, both files compile with the build's own flags, and it reverts
byte-identically. It needs your compile.

**Recorded in the issues (uncommitted):**
- **Blackrock Depths (810):** your three routes, three parties of five, each
  totalling 10 drops as in the plan above, with the rares always present. Bosses
  are placed on routes from my memory of the map, so it's worth a check.
- **The labyrinth (811):** packs dense enough that a pull usually draws a
  neighbour, and they keep spreading out while you're inside.
- **Campfires (170):** the 4- and 5-yard numbers above.
- **The gear chart (155k):** once the dungeon and raid item levels are settled,
  the "Level-60 Gear Ladder" page gets rebuilt wider. It's hand-written today;
  the rebuild will generate it from the plans' numbers like the other pages.

**Open questions:**
1. **Blackrock Depths tavern:** it's hostile to route 1's party but friendly to
   route 2's at the same moment. That means the tavern's people see each party
   differently. Is that what you want, or does route 2 start only after route 1
   has fought through?
2. **Pacing step:** how much harder for being ahead, for both the labyrinth and
   the joined runs? You weren't sure yet; it stays open.
3. **Campfire cap:** 4 or 5 yards gives about 8,100 or 3,400 spirit at the
   extreme. Is spacing alone enough, or should only the nearest few fires count?
4. **Fight length:** buddies count spirit's healing over a 30-second fight.
   Longer makes them prefer spirit more. Is 30 right?

--------------------------------------------------------------------------------

### User Request 20

can we also have raid bosses drop an enchanting vellum with a random enchantment
from around that level on it? Just for the low level dungeons. The higher level
ones give gems, I think. Or is it jewels? It shouldn't replace an item, but it
should still drop more from characters with large loot tables.

> 1. Blackrock Depths tavern: it's hostile to route 1's party but friendly to
> route 2's at the same moment. That means the tavern's people see each party
> differently. Is that what you want, or does route 2 start only after route 1
> has fought through?

yes they'll probably never be in the same room at the same time so

> 2. Pacing step: how much harder for being ahead, for both the labyrinth and
> the joined runs? You weren't sure yet; it stays open.

should be very harder to encourage pacing

would it be a good / bad idea to share the need/greed rolls between the entire
parallel groups? so a 15 man raid gets 15x more loot with 15x more competition.

> 3. Campfire cap: 4 or 5 yards gives about 8,100 or 3,400 spirit at the
> extreme. Is spacing alone enough, or should only the nearest few fires count?

and 6 yards?

> 4. Fight length: buddies count spirit's healing over a 30-second fight. Longer
> makes them prefer spirit more. Is 30 right?

sure.

--------------------------------------------------------------------------------

### Assistant Response 20

> Checking which existing issue covers higher-level dungeons dropping gems, and
> which enchanting scrolls exist in the game data, so the vellum drop builds on
> them.

Recorded everything, plus one new issue (812); none of these files are
committed. Your question about sharing need/greed rolls needs an answer, though,
so here's the comparison.

**What the server does today:** an item is rolled on only by raid members near
the corpse when it's looted (within reward range, about 74 yards). Parties in
different rooms never see each other's rolls.

**Shared rolls across the whole 15-person run, compared with each party rolling
its own:**

| | Each party rolls its own | The whole run rolls together |
|---|---|---|
| Items per person per run | 10 items ÷ 5 people = 2 | 30 items ÷ 15 people = 2 |
| Who gets an item | the best fit among 5 | the best fit among 15 |
| Items nobody in the party wants | more often greeded or wasted | rarer, since 15 people are asking |
| Roll windows per person per run | about 10 | about 30, three times as many |
| How unevenly loot lands | more uneven (small pool) | more even (larger pool) |
| Server work | none | rolls offered raid-wide regardless of distance, and the winner's item delivered to their bags, since they may be rooms away |

The average stays exactly the same: 15 times the loot shared by 15 times the
people. What changes is that items land with whoever wants them most across all
three routes, at the cost of three times as many roll windows. With mostly
buddies, their clan rules would make most of those rolls automatically, so the
extra windows fall mostly on the real players. The question stays open in 810
until you decide.

**Recorded:**
- **Scrolls from dungeon bosses (812):**
  - Bosses of the low-level dungeons drop extra "Scroll of Enchant" items on top
    of their loot, never instead of an item.
  - A boss draws among scrolls near its own level. A scroll's level comes from
    how much Enchanting skill its recipe needs, at about skill ÷ 5 = character
    level.
  - Bosses with more possible items drop more scrolls.
  - Gems were the right word for the higher end: Outland's monsters already drop
    uncut gems (155p). In this expansion "jewel" means only the jewelcrafters'
    Dragon's Eyes.
- **Blackrock Depths' tavern (810):** hostile to one party and friendly to the
  other at the same time, per party.
- **Pacing:** much harder for being ahead, steep enough that pulling ahead stops
  paying.
- **Campfires (170):** at 6 yards apart about 40 fires fit in the 20-yard
  circle, giving about 1,600 spirit at the extreme.
- **Fight length (617j3):** stays 30 seconds.

**Open questions:**
1. **Scrolls (812):** up to which level do they drop: 30, or up to the level-60
   dungeons? And how many per boss: one, and two for the most varied?
2. **Shared rolls:** yes or no, given the comparison above.
3. **Campfire cap:** is spacing alone the cap (3, 4, 5 or 6 yards), or should
   only the nearest few fires count?
4. **Pacing step:** the exact step. For example, every 10 points ahead adds 3
   levels to monsters not yet in combat?

--------------------------------------------------------------------------------

### User Request 21

can we add an issue file to add a wikipedia style page generated from the issue
files and such? It should also contain a full database, with all the times the
item is affected listed as a link to the transcript where the edit took place.
So you can see why and how something changed. We should have two different types
of sections for the default things, and the changed things.



<pasted_content id="7f61">
  ├─────────────────────────────────┼──────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Server work │ none │ rolls offered raid-wide regardless of distance,
  and the winner's item delivered to their bags, since they may be rooms away
  │
  └─────────────────────────────────┴──────────────────────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
</pasted_content id="7f61">


different instance too. Might even be on a different server.

> - Campfires (170): at 6 yards apart about 40 fires fit in the 20-yard circle,
> giving about 1,600 spirit at the extreme.

that's fine. But it only takes effect outside of combat, and only near a duel,
to counteract the extra damage from monsters. AH I DON'T KNOW I don't want
players lighting so many fires! Ah! What do you think?

> 1. Scrolls (812): up to which level do they drop: 30, or up to the level-60
> dungeons? And how many per boss: one, and two for the most varied?

up until level 60 dungeons. not in raids.

>   2. Shared rolls: yes or no, given the comparison above.

ideally, yes.

> 3. Campfire cap: is spacing alone the cap (3, 4, 5 or 6 yards), or should only
> the nearest few fires count?

good point. Let's say the nearest 6 campfires can count. Or rather, it stacks to
6. That might be the easiest implementation.

> 4. Pacing step: the exact step. For example, every 10 points ahead adds 3
> levels to monsters not yet in combat?

what do you mean 10 points? Like... 10 percent more of the way through the raid?
Maybe every 20 percent... How about it starts at 10 % and scales up to 20% over
the course of the dungeon. So early bosses have less of a penalty, but later
bosses are much stronger unless your allies are also attacking their core.

--------------------------------------------------------------------------------

### Assistant Response 21

> Writing the wiki page as an issue, plus your other answers. Getting the next
> free number first.

> Tidying a leftover line from that edit in the sparring-grounds issue.

I wrote the wiki as issue 171 and recorded your other answers; none of these
issue files are committed.

## The wiki (issue 171)

A generated encyclopedia with one page per item, creature, quest, spell, zone
and dungeon, linked to each other and to the issues. Every page has two kinds of
section:
- **Stock:** the thing as the game ships it, read from a stock copy of the world
  database.
- **Changed on Everland:** each change the project makes to it, newest first:
  before and after, which install step or patch makes it, the issue that asked
  for it, and a link to the line of the conversation where it was decided.

It's rebuilt by one command and never edited by hand. The install steps already
save the original values before changing anything, so those saved tables become
the record of what changed.

Questions:
1. A page for every stock thing (tens of thousands), or only changed things plus
   what links to them?
2. Where do rule changes made in C++ go (for example "spirit heals by stamina"):
   on the stat's page, the class's, or both?

## Campfires: what I think

The worry is one person lighting a field of fires. The cap of 6 already limits
the reward: at most 6 fires count, so at most 36 spirit, and only out of combat
near a duel.

What remains is someone lighting six fires alone to get the full 36. I'd add one
more rule: **count at most one fire per person who lit it.** Then reaching six
fires takes six people resting together, which is the picture you described:
allies boosting each other by the fire. It's in the issue as an open question.

## Recorded

- **Scrolls (812):** from every dungeon up to the level-60 ones, never raids.
- **Shared rolls (810):**
  - **Who rolls:** yes, every member of a joined run rolls on everything it
    drops, wherever they are: another room, another instance, even another
    server.
  - **Delivery:** the winner gets the item in their bags.
- **Campfires (170):** stack to 6, count only out of combat and near a duel, to
  offset the double damage from monsters.
- **Pacing (810):** a side that's ahead faces monsters 10% stronger near the
  start of the dungeon, rising to 20% near the end, applied only to monsters not
  yet fighting.

## Open questions

1. **Pacing:** is the 10–20% for any lead at all, or for each step of lead
   (say every 10 percentage points of progress ahead of the slowest side)? And
   "stronger" in stats (health and damage), or in levels?
2. **Campfires:** one fire per person, yes?
3. **Scrolls:** how many per boss? One, and two for the most varied?

--------------------------------------------------------------------------------

### User Request 22

> - Changed on Everland: each change the project makes to it, newest first:
> before and after, which install step or patch makes it, the issue that asked
> for it, and a link to the line of the conversation where it was decided.

maybe we can have the patch file update the wiki page? No need to dismantle the
wiki page after undoing the source-patch.

> 1. A page for every stock thing (tens of thousands), or only changed things
> plus what links to them?

full database, extracted from mysql during compilation

this includes things like... altered placements for NPCs, or changes to item
drop rates or whatever...

> 2. Where do rule changes made in C++ go (for example "spirit heals by
> stamina"): on the stat's page, the class's, or both?

for that one, on the stats page. Also in a "changes" page sorted by generally
how major the feature is.



<pasted_content id="7f61">
> The worry is one person lighting a field of fires. The cap of 6 already limits
> the reward: at most 6 fires count, so at most 36 spirit, and only out of
> combat near a duel.
>
> What remains is someone lighting six fires alone to get the full 36. I'd add
> one more rule: count at most one fire per person who lit it. Then reaching six
> fires takes six people resting together, which is the picture you described:
> allies boosting each other by the fire. It's in
>   the issue as an open question.
</pasted_content id="7f61">


mmmmm nah how about when you light a fire, if there's another fire of yours then
it douses that one and lights the new one. They should last until you move more
than their buff radius away.

> 1. Pacing: is the 10–20% for any lead at all, or for each step of lead (say
> every 10 percentage points of progress ahead of the slowest side)? And
> "stronger" in stats (health and damage), or in levels?

I don't knoooooowwwww

>   3. Scrolls: how many per boss? One, and two for the most varied?

just one each time

Also... I don't think there's any profession that can create staffs and off-hand
items. Is that true? If so, can we make Enchanting do that too?

How difficult would it be to make a custom libram / totem / idol etc?

--------------------------------------------------------------------------------

### Assistant Response 22

> Checking which professions make staves and off-hand items in this game data,
> before answering. The check reads the client's spell and skill tables for
> every recipe that creates a staff or an off-hand item.

Everything is recorded, and I've opened issue 172 for Enchanting making staves
and off-hands. None of the issue files are committed.

**Recorded:**
- **Wiki (171):**
  - **Patches write their own entries:** each source patch, install step and
    config patch writes its wiki entry when it applies. Undoing the patch leaves
    the entry, so the wiki is a history.
  - **Full database:** every item, creature, quest and spell gets a page,
    extracted during the build, including moved creature spawns and changed drop
    chances.
  - **Rule changes:** a C++ change like "spirit heals by stamina" goes on the
    stat's page, and on a Changes page sorted by how major the feature is.
- **Campfires (170):** lighting a fire douses your previous one, and a fire goes
  out when you move more than 20 yards (its reach) from it. So one fire per
  person.
- **Scrolls (812):** one per boss kill.
- **Staves and off-hands (172):** you're right. No profession makes a staff;
  only Inscription makes off-hand items (14 recipes, the expansion's tomes).
  Basic turned Inscription into a buff-scroll profession, so whether those 14
  are still taught there needs checking. The issue proposes Enchanting recipes
  for both.

**Pacing:** since you weren't sure, I put a first setting to try into issue 810,
to tune later in the balance log. For every 10 points of progress a side is
ahead of the slowest side, its monsters not yet fighting gain 10% health and
damage near the start of the dungeon, rising to 20% near the end. I chose stats
over levels because level gaps also change hit and crit chances. Basic already
caps those gaps at 3 levels, so levels would be a lumpier dial.

**How hard is a custom libram, totem or idol?** It depends on what it does:
- **Easy, no client change: stats only** (say stamina and spell power). The
  server can add a new item row, and the 3.3.5 client asks the server about
  items it doesn't know. The item needs an icon the client already has, which is
  fine since there are plenty of relic icons to reuse.
- **Easy: an existing relic's effect with new stats.** For example, "your
  Rejuvenation heals more," reusing a spell the client already knows. The
  effect's numbers stay fixed, since they live in the client's spell data.
- **Harder: a new effect.** The server can define new spells in a database
  table, and they'll work in game. But the client doesn't know them, so the
  item's "Equip:" line can't describe the effect. The optional tooltip addon
  (719) or your custom client could fill that in.

So relics with stats, or with existing effects, are a few database rows and work
today. A truly new effect works too, but its tooltip stays silent until there's
a way to show it.

**Open questions:**
1. **Wiki:** who decides how major a change is, for sorting the Changes page: a
   field in each patch's header (major, minor, tuning), or the size of what it
   touches?
2. **Enchanting crafts (172):** real recipes in the profession window need spell
   ids the client already knows, so we'd reuse unused ones. The alternative is a
   crafting menu at the enchanting trainer, in Lua with no client change. Which
   way?
3. **Enchanting crafts (172):** what item level should the staves and off-hands
   reach: like Enchanting's other products, or up to the dungeon blues?
4. **Pacing:** is the starting setting above worth trying?

--------------------------------------------------------------------------------

### User Request 23

> - Staves and off-hands (172): you're right. No profession makes a staff; only
> Inscription makes off-hand items (14 recipes, the expansion's tomes). Basic
> turned Inscription into a buff-scroll profession, so whether those 14 are
> still taught there needs checking. The issue proposes
>     Enchanting recipes for both.

how about inscription makes the precursor ingredient (runed staff) and
Enchanting makes the equipment

> - Easy: an existing relic's effect with new stats. For example, "your
> Rejuvenation heals more," reusing a spell the client already knows. The
> effect's numbers stay fixed, since they live in the client's spell data.

let's consider this option impossible, because it modifies something the player
can't see.

> - Harder: a new effect. The server can define new spells in a database table,
> and they'll work in game. But the client doesn't know them, so the item's
> "Equip:" line can't describe the effect. The optional tooltip addon (719) or
> your custom client could fill that in.

this one as well, because it hides information.

> - Easy, no client change: stats only (say stamina and spell power). The server
> can add a new item row, and the 3.3.5 client asks the server about items it
> doesn't know. The item needs an icon the client already has, which is fine
> since there are plenty of relic icons to reuse.

we should consider making idols be part inscription, part leatherworking or
herbalism (spec), and totems be mining and librams be enchanting and ranged
weapons do blacksmithing thrown gets 2x as many recipes than bows and crossbows
and such - any others?

> 1. Wiki: who decides how major a change is, for sorting the Changes page: a
> field in each patch's header (major, minor, tuning), or the size of what it
> touches?

use your best judgement

> 2. Enchanting crafts (172): real recipes in the profession window need spell
> ids the client already knows, so we'd reuse unused ones. The alternative is a
> crafting menu at the enchanting trainer, in Lua with no client change. Which
> way?

ideally, real recipes. fallback implementation would be the crafting menu, if
it'd work. And we'd have to craft them at a place of deep spiritual value, like
the altar of storms or a grand cathedral.

> 3. Enchanting crafts (172): what item level should the staves and off-hands
> reach: like Enchanting's other products, or up to the dungeon blues?

ya up to level 60

can we add an issue file to the custom client for the options menu and it's
settings? We should allow all resolutions up to ultrawide. The user should be
able to type in a value if they want. Or there's a drop-down with common
increments.

god... custom client is such a priority

can we work on that until it's done? I'm thinking something as simple as a game
character able to move about the world exactly as the server expects. Same
control scheme, no UI for now. q to quit.

specifically, a night-elf in ashenvale. They should move at 120% speed.

You know the in-game UI elements in World of Warcraft? I love those. We should
build an image generator for those, if we possibly can. It could build a UI for
us like we were assembling our own... Neato. Anyway, we should be able to draw
them like gizmos. "scrollbar" and "x_button" and "clicking-to-drag" etc that
handle common UI patterns. Then we should draw the UI, when it exists, very flat
and possibly shaded.

no need for most of the UI for the custom client. We just want to be able to
load vmaps and such and load all the models and display it in 3d space.

in the RGPL. fan-artists and copyright rulebreakers should be able to sue. If
the person wants to defend the copyright, they have to pay to make the art
suitably different. They will be compensated with the ability to buy the artwork
in question.

at all times, it must have proper credits and references to prior art.
fanfiction kills the copyright industry. torrenting kills the movie industry. Do
fewer films get made? No. They are for all of us.

... anyway, can we actually handle buddy-bot inventories in lua, and just fake
talking to the NPCs in the town? Same for their spell learning.

also buddy-bots shouldn't loot corpses. The items that are on them when the
creature despawns (if you left it behind) gets added to an appropriate
buddy-bot's inventory. We'll need an event that's emitted when creatures
despawn, and only if they had an inventory item. appropriate goods should be
given to other buddy-bots and the player.

but, how about, a nearby buddy-bot still walks up to every corpse and kneels
before it, randomly, once per corpse. The items are left on the body so the
player can grab them if they want, otherwise they'll get sorted into the
buddy-bot's inventory first based on need, then based on bag space. If someone's
low on bag space they can re-distribute, but only while kneeling around a
campfire, facing 45 degrees from the fire a yard or two away.

we should choose which buddy-bot to select with a random select without
replacement. But prioritizing the nearest buddy-bot by 2x. They'll have to
transfer items though - oh I guess I should say the items that despawn get put
into the inventory of the person who kneeled to grab them, or the player if they
checked and pulled the items before they despawned. The nearest buddy bot is a
"with replacement" shuffled in extra draw choice. Essentially guaranteeing it.
This means the tank does a lot of looting, but everyone else has just as much of
an even chance. It all depends on who's closest when the monster is slain. Keep
track of who should be assigned to who, and only go kneel when there's no
fighting going on. Visit all the corpses assigned to you, then move forward and
continue on.

the tank should move to the next mobs in a dungeon or raid automatically. The
buddy-bots should orient themselves around the tank (half circle though, facing
away from where the tank is facing, arcing behind the tank like a peacock
tailfeather display.) and do the thing where they only get a new waypoint when
the previous one is reached. The compass style follow mode will also be of use.
Same for the "goto" target spell. We will make great things with this system.

--------------------------------------------------------------------------------

### Assistant Response 23

> This round has a lot in it. You asked to keep going on the custom client until
> the first version is done, so that's the main work. The rest I'll write up as
> issues (plus your license idea, saved word for word as a note), then build the
> client.
>
> First, checking what this machine has for drawing 3D and what map data we have
> to load.

> Now the drawing side: a small matrix helper, the ground-mesh builder, and the
> main program.

> No virtual display here, so the screenshot run will flash the window on your
> screen for about two seconds. Writing the screenshot mode, the offline tests,
> and the run script.

> It ran and saved a picture, but at 637 bytes it's probably a single flat
> colour. Looking at it.

> Timed out. Probably my test broke itself: with the sky colour unused, sending
> it raises an error, and LÖVE's error screen waits instead of quitting.
> Keeping it in use and rerunning.

> As expected, the send fails. In the scratch copy I'm wrapping the sends so a
> dropped value is skipped instead of stopping everything.

*[commit] c0bf83e25 in wow-chat-2026 - First light for the custom client: a night elf walks Ashenvale on the server's own ground*

> First light is committed. Issues 173c and 173d were left out of that commit
> because a script wrote them; committing them now.

*[commit] f2ba333c0 in wow-chat-2026 - The custom client's next two steps: interface pieces drawn by name, and models*

The custom client's first light runs: a stand-in night elf walks Ashenvale
around Astranaar at 120% speed, with the game's controls and q to quit. It isn't
yet compared with the running server, and she's a violet box until models come.

**Run it:** `scripts/custom-client`

**Controls:**
- W / S: forward and back.
- A / D: turn; with the right mouse button held, they strafe.
- E: strafe right. Q would normally strafe left, but here it quits.
- Space: jump.
- Left mouse drag: turn the camera.
- Right mouse drag: turn her and the camera together.
- Mouse wheel: zoom.

**What it does:**
- **The ground** comes from the same map files the server uses for heights,
  divided into triangles exactly as the server divides it. The heights match the
  server's own arithmetic: Astranaar's flight point comes out at 106.7 yards.
- **Drawing:** the ground is sunlit, forest green going to grey rock on steep
  slopes, with fog in the distance. The tiles around her load as she walks.
- **She's a stand-in:** a violet box her height (2.3 yards) with a pale patch
  for her face, so you can see which way she's facing.
- **Speeds:** 120% of the game's base: 8.4 yards a second running, 5.4 backing
  up, half a circle a second turning, the game's gravity for jumps and falls.

**Checks:**
- **Movement and ground:** `scripts/test-custom-client` runs 14 checks offline,
  all passing: running speed, backing speed, strafing, turning, jump height,
  landing, slopes, cliffs, and no seams where tiles meet.
- **A picture:** `scripts/custom-client --shot` draws a few frames, saves a
  picture and quits. That's how I looked at it. I turned off edge smoothing,
  because with it on the saved picture comes out black.

**Still missing:**
- **A point-for-point match with the server.** It needs the server running and a
  way to ask it for the ground height at a spot. It was down.
- **Her real model** (issue 173d). That needs reading the game's data archives
  and its model format. It's the biggest next piece.
- **Slopes too steep to climb.** The game blocks slopes steeper than about 50
  degrees, and the client doesn't yet.

**Also written up this round** (uncommitted except the client issues):
- **Inscription makes blanks, Enchanting finishes them (172):** Inscription
  makes runed staves and blanks; Enchanting turns them into staves and off-hands
  up to level 60, at a place like the Altar of Storms or a cathedral. Real
  recipes first, a crafting menu as the fallback.
- **Crafted relics and ranged weapons (174):** relics carry stats only, since
  effects the player can't read are out.
  - Idols: Inscription plus Leatherworking or Herbalism.
  - Totems: Mining.
  - Librams: Enchanting.
  - Ranged weapons: Blacksmithing, with twice as many thrown recipes.
- **The custom client:** the options menu (173b), any resolution up to
  ultrawide, typed in or picked from a list; and the interface pieces drawn by
  name (173c).
- **Buddy errands in Lua (617u):** they still walk to the vendor or trainer,
  then a script does the trade.
- **Kneeling at the fallen (617v):** buddies don't loot. One kneels at each
  corpse, and the items go to it when the corpse despawns, or to you if you took
  them first. Buddies rearrange bags only kneeling around a campfire.
- **The tank leads (617w):** the tank advances on its own, with the buddies
  fanned out behind it like tail feathers.
- **Your license idea** is saved word for word in `notes/rgpl.md`.

**Open questions:**
1. **Next for the client:** go straight on to her real model (173d), or first
   check heights against the server once it's up?
2. **Steep slopes:** add the game's limit now, or later?
3. **Where the client lives:** inside this project as now
   (`src/custom-client/`), or as its own project?
4. **Sigils:** who makes the death knights' relics, Inscription or
   Blacksmithing? And do wands stay Enchanting's and guns Engineering's?

--------------------------------------------------------------------------------

