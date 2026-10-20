# 609 — Losing the Connection

*Written 2026-10-16 from the clean-room checklist's note 10.2
([docs/057](../docs/057-what-the-original-program-decides.md)).*

## Blocked by

- [707](./707-saved-variables.md) (the interface shutdown a lost
  connection runs), the world connection (Phase 5, not yet written as
  issues), the login screens (sketched in this phase's capstone, not
  yet written as issues).
- [602](./602-login-results.md).

## Blocks

- [608](./608-phase-demo.md).

## Current behavior

Nothing exists. There's no connection to lose.

## Intended behavior

When the game server's connection drops, the client cleans up, saves and
returns to the login screen with the same messages and events as the
original (note 10.2):

- **Clean-up first:** connection clean-up, no longer connected or
  logging out, the anti-cheat module unloaded.
- **Planned disconnects** (the client asked for them): no message; with a
  reconnect wanted, the status dialog with Cancel and
  `GAME_SERVER_LOGIN`, and a new connection.
- **A drop from a connection that isn't the current one** is ignored.
- **Otherwise:** the world torn down with the full interface shutdown
  (`PLAYER_LOGOUT`, saved variables written, 707); the mouse released; a
  suspended login screen resumed; `DISCONNECTED_FROM_SERVER` with a
  reason number if past account login.
- **During account login:** `DISCONNECTED_FROM_SERVER` after a finished
  operation (realm choice cleared), else `OPEN_STATUS_DIALOG` with OK
  and the pending status text.
- **Status names:** result and status codes 0, 1, 2 and up turned into
  the interface's string names in the original's order (`RESPONSE_SUCCESS`,
  `RESPONSE_FAILURE`, ...), shown in the player's language.
- Going idle for 30 minutes logs out separately
  ([704](./704-events-and-the-per-frame-update.md)).

## Suggested implementation steps

1. The drop handler with its three paths (planned, stale, unplanned).
2. Tearing down the world through 707's shutdown.
3. Back to the login screen, with the events and the status dialog.
4. The code-to-name table.
5. Tests (with a test server that closes the socket):
   - an unplanned drop in the world writes saved variables and fires
     `PLAYER_LOGOUT`, then `DISCONNECTED_FROM_SERVER`;
   - a drop from an old connection does nothing;
   - a planned reconnect shows the status dialog with Cancel.

## Open questions

None. The failed-login messages, beyond the code names, wait on
docs/057.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) note 10.2,
  [707](./707-saved-variables.md).
