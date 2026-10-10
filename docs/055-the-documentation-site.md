# 055 — The Documentation Site and Its Note Box

The project's documents, issues, notes and code as one browsable site in
`docs/HTML/`, and the path a reader's note takes from any page to the
project's own mailbox. Two data paths: **building** (files on disk become
pages) and **a note** (a reader's words become a file in the project
mailbox's inbox, then is sorted, forwarded and, if public, shown on the site).

## Building: files become pages

```
docs/  notes/  issues/  src/  tests/  scripts/  CLAUDE.md, desire/ faith/ strategems/ ...
        │  read whole, one level deep per folder (scripts/build-docs, "SECTIONS")
        ▼
pages: one per file  (path, title, text, kind = Markdown text or code, section)
        │  names indexed: issue numbers, file names, module names, docs/NNN
        ▼
Markdown → HTML  (scripts/libs/docs-markdown.lua)
        │  asks the builder, for every link and every name in the prose,
        │  "is this a page?"; an issue number, a file or module name
        │  becomes a link; a link to nothing is drawn with a wavy underline
        ▼
measured numbers  (issues per phase by status, lines per engine part)
        │  written into the pages as data; docs-site.js draws them
        ▼
docs/HTML/*.html  + docs-site.css, docs-site.js, site-config.js
        │
        ▼
the report: links to nothing, issue numbers that belong to other
projects, pages nothing in the prose points at
```

- **One flat folder**: every page is named after the path it came from
  (`src/013-errors.h` → `src-013-errors-h.html`), so every link is one
  rule, and the contents list down the left of every page reaches every
  other page.
- **Never edited, never committed.** Rebuilt from the sources by
  `scripts/build-docs` (about a second); git ignores `docs/HTML/`, since each
  page carries the mailbox's key. Re-run it after changing any document.
- **Which numbers are issues.** In `issues/` and the roadmap, every number
  that is one of this project's issues links to it (they are tables of issue
  numbers). Elsewhere only a number after "issue", "issues" or "#" does. It
  never links when another project's name is just before it
  (`world-edit-to-execute issues 901-912`), when it is a date's year, or
  on the pages copied from wow-chat-2026's conversations, whose numbers are
  that project's.
- **Which names are files.** A path (`scripts/build`), a module number
  (`013-errors`, which leads to what the part is for, its `.info.md`), or a
  file name with a dash or a dot (`build-docs`, `013-errors.h`). A bare
  word like "run" never links, even though `scripts/run` exists.
- **Generated pages**: the front page (`index.html`), progress by phase
  (each phase's issues, by the status its progress table gives them), and
  the engine's parts by size (lines of each part's `.c` and `.h`). The
  counts are measured on every build; run the builder for current numbers.
- **Rebuilt after every commit** by `scripts/git-hooks/post-commit`, once
  `scripts/install-git-hooks` has linked it into the checkout.
- **The look**: night ground, moonlit text, the original interface's gold
  for links; a light (parchment) theme on the ◐ button, remembered in the
  reader's browser. `/` jumps to the contents filter.

## A note: a reader's words become a file in the inbox

```
the reader types, ticks boxes, presses send
        │  docs-site.js, "the form"
        ▼
the note's text                       page: docs/045-datapath.md
                                      tag: bug            (0 or more: bug, feature, fan-mail)
                                      public: no
                                      ---
                                      the reader's words
        │  wrapped as rmail's request:  POST /deliver, JSON body
        │  { type "message", subject "feedback-<page>-<time>-<6 letters>",
        │    message_id (random), body (the note), mtime (now) }
        ▼
sealed: rmail's frame, built in the browser by docs-seal.js
        [4-byte length][12-byte nonce][AES-256-GCM sealed request + 16-byte tag]
        key = SHA-256 of the docs-feedback contact's token (site-config.js)
        │  fetch(), mode no-cors: no reply is read, so no permission is asked
        ▼
on the wire: the browser's own request around the frame
        POST / HTTP/1.1, its headers (a few hundred bytes), a blank line, the frame
        │  MAILBOX_HOST:5557 (scripts/build-docs' second argument; 127.0.0.1 by default)
        ▼
the project mailbox's rmail daemon (project-rmail/)
        │  the first byte is a capital letter, so a web request: steps past
        │  the headers (rmail 6d2ede2, 6ff91dc), then as for any contact:
        │  tries each contact's key, finds docs-feedback, handles /deliver
        ▼
scripts/check-docs-feedback  (rmail's on_receive_raw hook, and on_update for edits)
        │  sender not docs-feedback → passed through unchanged
        │  every header line checked against scripts/libs/feedback-form.lua;
        │  terminal-fooling bytes removed; too long → refused
        │  prints the cleaned note, or a refusal saying why (never nothing:
        │  printing nothing would make rmail keep the original)
        ▼
project-rmail/inbox/feedback-<page>-<time>-<letters>
        first line: "checked: <when> by scripts/check-docs-feedback, ..."
        refusals also: tmp/shared-memory/docs-feedback-refusals.log
        │
        ▼
scripts/sort-docs-feedback  (rmail's on_receive hook, once the note is saved)
        │  from the owner → nothing (no loop)
        │  else: project-rmail/outbox/<same name>, addressed
        │        "to: owner"                 always
        │        "to: website"               a checked note with "public: yes"
        │  log: tmp/shared-memory/docs-feedback-sorting.log
        ▼
rmail sends the owner's copy to ~/mail (contact "owner", 127.0.0.1:8025);
the website has no address, so its copy waits in the outbox, sorted
        │  (the owner deletes their copy → rmail drops the "to: owner" line;
        │   a private note's outbox file then goes; a public one stays)
        ▼
scripts/build-docs gathers every outbox file addressed to the website
        → notes.html (folders by page, kind or month; newest or oldest first)
        → the foot of the page each note is about
        (a stranger's text: escaped in pages, put on screen only as text)
```

The names in this path (the mailbox's own, its port, the three contacts:
`docs-feedback`, `owner`, `website`) are written once, in
`scripts/libs/project-mailbox.lua`, and every script reads them there. The
owner's main mailbox knows this one as `w-client-docs`;
`scripts/connect-owner-mailbox` wrote one shared key into both contacts
files (2026-10-08; the owner: "~/mail, sure, edit the contact file").

### Sealing on plain http

The site is to be served over plain http from a server (the owner,
2026-10-08: "no https, not just local"). Browsers give a page their
built-in encryption (`crypto.subtle`) and their id maker
(`crypto.randomUUID`) only when the page is "secure": https, the machine
itself (localhost), or a local file. Measured in Firefox on a page served
from this machine's network address: `isSecureContext` false, `subtle`
undefined, `randomUUID` undefined, `getRandomValues` present. So every page
seals with `docs-seal.js` instead, SHA-256 and AES-256-GCM written out in
plain JavaScript, taking only randomness from the browser. It is used on
every page, secure or not: one way of sealing, one way to fail.
`tests/052` checks it against the standards' published answers and has
rmail's own decryption (OpenSSL) open what it sealed.

### What rmail does and does not do yet (2026-10-08)

Done by rmail's maintainers and measured here (`tests/052 --real-mailbox`,
`tests/056`): a page's wrapped note reaches the real daemon, passes the
gatekeeper and lands in the inbox; openings that are neither a message nor
a web request are dropped at once.

Where rmail stands, each part in an rmail issue:

- **The pages' contact may only send plain messages** (rmail issue 420,
  extended 2026-10-08, built 2026-10-09 in rmail commit 9ec702e; the mark
  is the `listen = true` this mailbox already writes). The owner: "we
  have exactly one contact, which is the website client. It should refuse
  deletes, address changes, attachments, and anything other than a plain
  message from that contact." rmail now checks every request from a
  listen-only contact before handling it, whether bare or wrapped by a
  browser: only `POST /deliver` with a plain message (type, subject, a
  body of at most 64 KiB, message id, time) is taken; a delete, an edit,
  attachments, an address change or anything else is answered 403,
  logged, and changes nothing. Such a contact is also never sent to.
  `tests/056` sends an edit carrying terminal-control bytes and checks that
  rmail refuses it and the note is left as it arrived. The gatekeeper
  stays on rmail's `on_update` hook as well, in case a contact without the
  mark ever sends an edit.
- **The router check is gone** (rmail issue 411, reopened and built
  2026-10-08, commit cac3d72). Before it, once it held its port, rmail
  probed the router whenever it could find `upnpc` or `natpmpc` (first in
  its own `deps/bin/`, where it keeps a `natpmpc`, then the program path):
  UPnP got a test port mapping added and removed, NAT-PMP one query, and an
  open router meant a message to every contact. The owner: "We shouldn't
  open any ports automatically unless specifically requested to. Also we
  shouldn't probe the network unless requested." Now a mailbox never probes
  at start (rmail's `scripts/test-router-left-alone.sh` proves it), and
  `scripts/run-project-mailbox` refuses to start an rmail older than that.
  (While it waited, the start script hid both programs from rmail. A first
  version of `tests/052 --real-mailbox` hid them from the program path
  only; rmail still found its own `natpmpc`, so that run sent the router
  one NAT-PMP query. It opened nothing, and rmail logged nothing when no
  router answered, which is why it went unseen until the start script
  checked.)
- **Bodies past 128 KiB skip the checker.** rmail starts a hook as a
  program with the body as one of its arguments, and Linux refuses to
  start a program with any single argument longer than 128 KiB
  (`MAX_ARG_STRLEN`, 32 pages of 4 KiB). The network side is not the
  limit: rmail reads bodies up to 50 MB. The start fails, rmail reads the
  hook's output as empty, and an empty output means "keep the body as it
  came". Asking the sender to send again would change nothing, since the
  same body would fail the same way. The fix is in how rmail hands the
  body over (standard input, or a file, both unlimited) and in treating a
  hook that failed to run as a refusal: rmail issue 215 (open). Meanwhile
  the pages' contact cannot reach it: rmail takes at most 64 KiB of body
  from a listen-only contact, and the gatekeeper at most 8000 bytes of words.
- An inbox file the daemon has no record of is overwritten by an arriving
  message with the same subject. The pages' subjects carry the second and
  six random letters, so honest notes never collide; a stranger could aim
  at a name. Not yet in an rmail issue.

## The pieces

| file | what it does |
|------|--------------|
| `scripts/build-docs` | gathers the files and the public notes, indexes the names, measures, writes the site, reports |
| `scripts/libs/docs-markdown.lua` | Markdown and code to HTML; asks the builder about links |
| `scripts/libs/docs-site.css` | the look of every page |
| `scripts/libs/docs-seal.js` | SHA-256 and AES-256-GCM in plain JavaScript, for pages the browser does not trust |
| `scripts/libs/docs-site.js` | theme switch, contents filter, the note box, the charts, the notes viewer |
| `scripts/libs/feedback-form.lua` | the note's boxes and limits, read by both the builder and the checker |
| `scripts/libs/project-mailbox.lua` | the mailbox's name, port and contacts' names, read by every script |
| `scripts/make-project-mailbox` | makes or updates `project-rmail/`: the config every time, the contacts once (never a new key) |
| `scripts/connect-owner-mailbox` | one shared key into the project's and the owner's contacts files |
| `scripts/run-project-mailbox` | starts the mailbox (refusing an rmail that still probes the router) |
| `scripts/check-docs-feedback` | the mailbox's gatekeeper for notes, new and edited |
| `scripts/sort-docs-feedback` | files each saved message in the outbox: the owner always, the website when public |
| `scripts/git-hooks/post-commit` | rebuilds the site after every commit |
| `scripts/install-git-hooks` | links the project's hooks into `.git/hooks/`, once per checkout |
| `tests/051-test-docs-feedback-checker.lua` | the gatekeeper against honest and hostile notes |
| `tests/052-test-note-box-frame.sh` | a real page served over plain http, end to end: a stand-in mailbox, or `--real-mailbox` a throwaway rmail daemon |
| `tests/053-unseal-note-frame.lua` | opens a captured note with rmail's own decryption, runs the gatekeeper |
| `tests/054-catch-one-post.lua` | the stand-in mailbox: keeps one posted note, answers 200 |
| `tests/056-test-mailbox-sorting.sh` | two throwaway mailboxes: checking, sorting, forwarding, the edit hole, and the public notes on the built site |

## Running it

```bash
scripts/make-project-mailbox            # makes it, or updates its config; never replaces a key
scripts/connect-owner-mailbox           # once: introduces it to ~/mail
scripts/run-project-mailbox             # runs it (until stopped)
scripts/install-git-hooks               # once per checkout: rebuild after each commit
scripts/build-docs                      # by hand
scripts/build-docs --host example.org   # pages that post to a mailbox on a server
tests/051-test-docs-feedback-checker.lua
tests/052-test-note-box-frame.sh
tests/052-test-note-box-frame.sh --real-mailbox
tests/056-test-mailbox-sorting.sh
```

**Elsewhere later** (the owner, 2026-10-08: "Focus on making it local
capable now, with the potential to be hosted elsewhere planned for"). What
moves when the site is served from a server, possibly a Linode:
`build-docs --host` names the address readers' browsers post to;
`--mailbox` and `--out` let the build read a mailbox and write a site
anywhere; the mailbox's pages key is the one thing a site and its mailbox
must share; the owner's entry in `~/mail/contacts` gets the server's
address (rmail also announces new addresses itself); and
`run-project-mailbox` becomes the service's start command.

## Open questions

1. ~~**Starting the project mailbox here.**~~ Answered 2026-10-09: not
   now, "we don't have the website set up yet, so there's no way to receive
   traffic" (the owner). It will run as a service once the site is up.
2. **The website contact's address.** Public notes are addressed to
   `website`, which has no address, so they wait in the outbox. When
   wow.ritzmenardi.com runs a mailbox, should `website` get its address so
   the notes travel there, or stay address-less, gathered by the build?
3. **The overwrite hole** (above): file it as an rmail issue?
