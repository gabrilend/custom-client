# 010 — Table of Contents

```
custom-client/
├── notes/
│   ├── vision                            # the original vision text
│   └── wow-chat-2026-conversations/      # copies of the conversations that built the client (README: where the live ones are)
│
├── docs/
│   ├── 000-vision-summary.md             # vision as actionable summary
│   ├── 001-architecture-overview.md      # process model, layering, threading
│   ├── 002-technology-stack.md           # C11, bash build, raylib, our MPQ reader, zlib, OpenSSL, LuaJIT
│   ├── 003-asset-formats.md              # MPQ, DBC, BLP, M2, WMO, ADT, …
│   ├── 004-network-protocol.md           # auth + world handshake & framing
│   ├── 005-rendering-pipeline.md         # frame structure, render passes
│   ├── 006-ui-and-addons.md              # FrameXML, sandboxing, addon API
│   ├── 007-audio-system.md               # mixer, 3D model, SoundEntries
│   ├── 008-build-system.md               # one bash build script, its outputs
│   ├── 009-roadmap.md                    # 11 phases, with capstones
│   ├── 010-table-of-contents.md          # this file
│   ├── 011-balance-updates.md            # append-only tweak log
│   ├── 012-asset-replacement-and-provenance.md  # resolver, similarity scores, seal, lineage
│   ├── 045-datapath.md                   # files → memory → card → screen, frame by frame (built)
│   ├── 046-ingame-checks.md              # what the client guesses, for the owner to check in the original
│   ├── 047-optimization-checklist.md     # the frame made fast, item by item, measured
│   ├── 048-stock-options.md              # the original's whole settings menu (generated)
│   ├── 055-the-documentation-site.md     # the site's build, and a reader's note from page to inbox
│   └── HTML/                             # the browsable site, built by scripts/build-docs (git-ignored)
│
├── issues/                               # tracked work, see naming below
│   ├── phase-1-progress.md
│   └── completed/
│       └── demos/                        # phase end-of-phase demos
│
├── src/                                  # the engine, C, files numbered in reading order
├── libs/                                 # vendored deps (raylib, when fetched)
├── assets/                               # proprietary MPQ chain (gitignored)
├── tests/                                # the test program (044); the WC3 picture parity check (050, LuaJIT);
│                                         #   the note gatekeeper (051); the note box end to end (052-054);
│                                         #   two mailboxes sorting and forwarding (056)
├── scripts/                              # build, run, generate-info, build-docs, make-project-mailbox,
│                                         #   check-docs-feedback, sort-docs-feedback, make-, connect-
│                                         #   and run-project-mailbox, install-git-hooks, …
│   └── git-hooks/                        # post-commit: rebuilds the documentation site
├── project-rmail/                        # the project's own mailbox: readers' notes (git-ignored;
│                                         #   made by scripts/make-project-mailbox, see docs/055)
│
├── input/   output/   desire/   faith/   strategems/
│                                         # see CLAUDE.md — long-lived notes
│
├── tmp/                                  # symlink → /tmp/azc-custom-client/ (exec tier);
│   └── shared-memory/                    #   → /dev/shm/custom-client/ (logs, dumps)
└── .file-index-counter                   # global indexed-filename counter
```

## Reading order

For a new collaborator, read in this order:

1. [notes/vision](../notes/vision)
2. [docs/000-vision-summary.md](./000-vision-summary.md)
3. [docs/001-architecture-overview.md](./001-architecture-overview.md)
4. [docs/002-technology-stack.md](./002-technology-stack.md)
5. [docs/009-roadmap.md](./009-roadmap.md)
6. Then dive into whichever of `003`–`008` matches the area you'll work on.
7. For the code as it is: [045](./045-datapath.md), then `src/` in number order (each file's `.info.md` first).

## Issue file naming

Format: `{PHASE}{ID}-{DESCR}.md`, e.g. `105-mpq-reader.md`.

- `{PHASE}` — 1 to 10.
- `{ID}` — sequential within the phase, 01 onwards.
- `{DESCR}` — dash-separated short summary.

Sub-issues for a large piece of work append a letter: `105a-...`, `105b-...`.

## Indexed filename counter

`.file-index-counter` holds the current highest index across **all** indexed
files in the project (docs, source, tests, scripts). Reads before
creation, writes after. Read it: `cat .file-index-counter` (it was 56 on 2026-10-08: docs 000 to 012, source 013 to 043, the test program 044, docs 045 to 048, the JPEG reader 049, tests 050 to 054, doc 055, test 056).
