# 602 — The Login Screen's Results

*Written 2026-10-16 from the clean-room checklist's note 10.4
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 6's capstone sketched for "login screen (background BLP,
two input fields, login button)". The screen's drawing is in that
sketch; this issue holds what the original decides about the answer
it gets back.*

## Blocked by

- Phase 5's login handshake (not yet written as issues).

## Blocks

- [608](./608-phase-demo.md), [609](./609-losing-the-connection.md).

## Current behavior

Nothing exists. There's no login.

## Intended behavior

Each answer from the login server is shown as the original shows it
(note 10.4):

- **Result numbers to string names:** the note's table, from 0 and 14
  (success; 14 adds a survey) through `LOGIN_BANNED`,
  `LOGIN_UNKNOWN_ACCOUNT` (and its PIN variants after the first two
  failures), `LOGIN_ALREADYONLINE` and the rest, to 255 `DISCONNECTED`;
  10 isn't a failure but a patch to download; any other number
  `LOGIN_FAILED`. The screen shows each name's text in the player's
  language.
- **Unknown account and wrong password show the same message.**
- **Names it has no entry for:** `LOGIN_UNKNOWN_` plus the number, and
  `LOGIN_STATE_UNKNOWN_` plus a stage's number.
- **Reconnecting** after a first login ignores the result and goes on to
  the realm data.

## Suggested implementation steps

1. The result table and the PIN-failure count.
2. The unknown-name fallbacks.
3. The reconnect path.
4. Tests: results 4 and 5 show the same text; result 40 shows
   `LOGIN_UNKNOWN_40`; result 10 starts a download, not an error; a
   reconnect ignores a failing result.

## Open questions

None.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) note 10.4,
  [609](./609-losing-the-connection.md) (which names the login screens
  among its blockers).
