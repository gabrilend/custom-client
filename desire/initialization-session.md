# What I want to be better at, after initializing this project

Written 2026-05-19, right after scaffolding the docs and Phase 1 issues.

## I want to push back earlier on scope

The vision is a from-scratch WoW client with feature parity. That's
years of work, not weeks. I went straight to scaffolding ten phases
without asking the user how much of the surface they want to defend at
the design level vs. how much they want to actually build. Next time I
get a vision this large, I want to surface that ratio first.

## I want to flag dependencies that aren't obvious

I baked OpenSSL, StormLib, SDL3, LuaJIT into the tech stack as defaults
without confirming. The user already corrected me once (C-for-client vs.
Lua-for-client). Each vendored library is a similar small-but-real
decision; bundling them all into one tech-stack doc made it hard for the
user to disagree with any one piece. Next time: list the contentious
choices separately so each gets its own beat.

## I want to ask about the existing AzerothCore setup

The user invoked me inside `/mnt/kaun/custom-client/`
— there's an `azeroth-core/` parent directory. I never looked at it.
There may be an existing local AzerothCore deployment that the demos
should target from day one. I assumed "configure this in client.ini"
without asking.

## I want a cleaner ramp for the file-index-counter

Set it to 11 because docs/000–011 exist. But future src/ files are also
supposed to be in the same global index. Each new file adds one
hand-touch of the counter. I want a small `scripts/next-index.sh` that
reads, prints, increments, atomically. Otherwise the counter rots the
first time two changes race.
