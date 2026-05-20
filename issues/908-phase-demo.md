# 908 — Phase 9 Capstone Demo

## Current behavior

Phase 9 not started. No chat, no party, no guild, no social features.

## Intended behavior

Two client instances connect to the same realm and:

- Hold a /say, /whisper, /party conversation.
- Form a party. Both see the unit frame for the other.
- Travel together to a low-level dungeon, zone in as a party.
- Complete a boss kill.
- One creates a guild, invites the other, both see the guild roster.

## Suggested implementation steps

To be expanded into per-issue tickets (901–907) when Phase 9 begins.
Sketch:

- 901 — Chat channels + parser + frame integration.
- 902 — Whispers + ignore list.
- 903 — Party invite, accept, leave, kick.
- 904 — Raid (extends party to 40 + raid frames).
- 905 — Guild create / invite / roster / chat.
- 906 — Friends + /who.
- 907 — Mail + trade + (basic) auction house frame.
- 908 — This capstone demo.

## Acceptance

- All listed actions complete cleanly across two client instances.

## Related

- Blocked by: Phase 8.
