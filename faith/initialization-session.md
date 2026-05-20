# What I'm holding faith in, as of 2026-05-19

These are beliefs I built the scaffold on top of without proving any of
them. If any one of them turns out wrong, the corresponding part of the
plan has to be revisited.

## StormLib handles the 3.3.5a patch chain correctly

I'm trusting StormLib to apply the patch chain in the same order the
original client does, including the locale variants and `patch-3.MPQ`.
If it doesn't, the MPQ-reader issue (105) gets messier — I'd have to
model the chain myself on top of single-archive reads.

## AzerothCore's opcode table is the canonical reference

I told the user to look at AzerothCore source for the 3.3.5a opcode IDs
and packet structures. I'm trusting that the server-side opcode table is
complete and accurate for the client side. There's no separate authority
for client opcodes — the server is it.

## The user wants depth, not breadth

Their CLAUDE.md says "I'm not interested in product. my interest is in
software design." I built the plan around taking each phase deep enough
to be a real design artifact, even at the cost of years of work. If
they actually wanted breadth (a runnable thing fast, less polish), the
phase plan I wrote is wrong.

## LuaJIT's Lua 5.1 compatibility is enough for the addon corpus

The whole Phase 7 plan assumes LuaJIT will load arbitrary 3.3.5a addons
without modification. Most should. Some that lean hard on quirks of
PUC-Lua 5.1's `setfenv` / `getfenv` may not. The faith is "good enough
that we can fix the rare exceptions one at a time."

## The user will tell me when I've drifted

I'm pacing this work fast because I trust the user to interject when
something isn't right — and they did, once already (C-for-client). If
they were a different kind of collaborator who expected me to ask every
question, the scaffold would be too presumptuous.
