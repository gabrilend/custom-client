# 815 — Send Companions to a Clicked Spot

*Moved here 2026-10-30 from wow-chat-2026, where it was written on
2026-10-04 as issue 617s6, "Send One Buddy" (part of that server's
"compass formation" of companion bots). The owner, 2026-10-30, told the
click is the part a server can't hear from the original client: "custom
client work. Can you move it there?"*

## Blocked by

- [809](./809-targeting-and-range.md) (a point picked from the screen).
- Phase 5 (units from the server, with their positions).

## Blocks

Nothing yet.

## Related

- [814](./814-box-select-in-the-wow-world.md), box select: picks *which*
  companions; this one says *where they go*. Built together, a drag picks
  them and a click sends them.
- wow-chat-2026's close follow (its issues 617s2 to 617s4: companions in a
  ring round the player, "circle me"), whose empty place a sent companion
  leaves until it is told to follow again.

## Current behavior

Nothing exists. The original 3.3.5a client has no way to tell the server
"this spot on the ground" except by casting a ground-targeted spell; the
bot module's `rtsc` command works that way (a click spell, the spot
saved under a number, then "rtsc go"). No addon can read the 3D point
under the mouse.

## Intended behavior

The owner names who goes and clicks where (2026-10-04): "@party or @name
to pick one specifically and @1 or @2 to select the first or second
character in your party and also we should be able to do as many as we
want at the same time." So:

- **Who**: `@party` (every companion in the group), `@<name>`, `@<n>`
  (the n-th character of the party, in the party list's order), several
  at once (`@1,3`, `@Pozeta,Muspo`), or those box-selected (814).
- **Where**: one click on the ground sends them at once (nothing saved,
  unlike `rtsc`). The client turns the click into a world point (the
  ground under the cursor) and tells the server: the companions named and
  the point, and the way the player faced.
- **On the server** (wow-chat-2026's companion module): several sent are
  spread on a small arc round the point, each its own place; each walks
  there on the navigation mesh, faces the way the player faced, and holds
  until told to follow (`@... follow`), or until the player walks 40
  yards away from them.

**Different from `rtsc` on purpose**, so the two can be watched side by
side (the owner: "Try to distinguish yourself from their implementation
in as many ways as you can"):

| | `rtsc` (bot module) | ours |
|---|---|---|
| choosing the spot | a click spell, saved under a number, then "go" | one click sends at once, nothing saved |
| several sent | to one saved spot, piling up | spread on a small arc round the click, each its own place |
| the way there | straight moves | the navigation mesh, the route drawn ahead |
| at the spot | stands | faces the way the owner faced when clicking |
| return | "rtsc go" again, or follow | `@... follow`, or walking 40 yards away from them |

## Suggested implementation steps

1. The ground point under the cursor (the picking 809 needs, aimed at the
   terrain and world models instead of units).
2. The message to the server: who, where, which way; how it travels (an
   addon message on the game connection is the first candidate).
3. The server's side, in wow-chat-2026: reading the message, the arc, the
   walk, the hold, the return (written there once this end exists).
4. Watched beside `rtsc` in game.

## Open questions

1. The click: a plain click on the ground with companions selected (as a
   strategy game does), or a held key plus click, so an ordinary click
   still moves the player?
