# Patterns I keep reaching for

Notes for future-me. Each of these showed up in more than one place
while I was scaffolding this project; they're worth keeping in active
memory when I'm planning the next bit of work.

## Patch-chain reads everywhere

I noticed I described "walk a list of providers back-to-front, first hit
wins" in at least four places: MPQ archive resolution, saved-variables
vs. defaults, config layering (defaults → ini → CLI), and addon load
order. Same shape, different domains. When I see a "this overrides that"
pattern coming, I should reach for this strategem first instead of
inventing a bespoke mechanism.

## Typed errors at every boundary

`err_t` everywhere instead of mixed `-1` / `errno` / `bool` conventions.
The cost is a per-function struct-ish discipline; the win is that every
call site reads as "what can fail here, by name." This pays back hardest
when debugging — `err_str(err)` collapses a whole class of "what does
this return value mean?" archaeology.

## One static lib per subsystem

Each `src/<subsystem>/` becomes its own static lib. Headers point
downward only; the linker enforces it. I keep wanting to flatten this
for convenience; resisting the urge keeps the layering legible. The
side effect: tests can link a single subsystem's lib without dragging
the whole world.

## SPSC ring as the only cross-thread channel

Network thread → main, streamer → main, main → audio callback. All
single-producer/single-consumer. A bounded ring of pointers into a
per-producer arena is the cheapest correct primitive. No locks, no
per-message allocation. When I'm tempted to reach for an mpmc queue or
a mutex, the right question is usually "can I split this into two SPSC
channels instead?"

## Scaffold demos as deliverables, not afterthoughts

Per CLAUDE.md, phase demos are part of the deliverable. I treated 108,
208, etc. as first-class issues with their own acceptance criteria from
the start, not as "build a quick test at the end." This shapes the
phase work — every issue knows it has to plug into the capstone, which
keeps random-feature-creep down.

## Notes to future-me

When I'm starting a new doc bundle for an unfamiliar domain (game
client, compiler, distributed system, whatever), the order that worked
here was: vision → architecture → tech stack → format catalogue →
roadmap → table of contents. Architecture before tech stack: the
"what's the shape" decision constrains the "what's it built on"
decision, not the other way around.
