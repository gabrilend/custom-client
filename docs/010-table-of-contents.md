# 010 — Table of Contents

```
custom-client/
├── notes/
│   └── vision                            # the original vision text
│
├── docs/
│   ├── 000-vision-summary.md             # vision as actionable summary
│   ├── 001-architecture-overview.md      # process model, layering, threading
│   ├── 002-technology-stack.md           # C11, CMake, raylib, OpenSSL, LuaJIT, …
│   ├── 003-asset-formats.md              # MPQ, DBC, BLP, M2, WMO, ADT, …
│   ├── 004-network-protocol.md           # auth + world handshake & framing
│   ├── 005-rendering-pipeline.md         # frame structure, render passes
│   ├── 006-ui-and-addons.md              # FrameXML, sandboxing, addon API
│   ├── 007-audio-system.md               # mixer, 3D model, SoundEntries
│   ├── 008-build-system.md               # CMake + scripts/ entry points
│   ├── 009-roadmap.md                    # 11 phases, with capstones
│   ├── 010-table-of-contents.md          # this file
│   ├── 011-balance-updates.md            # append-only tweak log
│   ├── 012-asset-replacement-and-provenance.md  # resolver, similarity scores, seal, lineage
│   └── HTML/                             # browsable doc bundle (TBD)
│
├── issues/                               # tracked work, see naming below
│   ├── phase-1-progress.md
│   └── completed/
│       └── demos/                        # phase end-of-phase demos
│
├── src/                                  # C client (per-subsystem dirs)
├── libs/                                 # vendored deps (StormLib, miniz, …)
├── assets/                               # proprietary MPQ chain (gitignored)
├── tests/                                # one test exe per subsystem
├── scripts/                              # build.sh, run.sh, test.sh, …
│
├── input/   output/   desire/   faith/   strategems/
│                                         # see CLAUDE.md — long-lived notes
│
├── tmp/                                  # symlink → /tmp/azc-custom-client/
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

## Issue file naming

Format: `{PHASE}{ID}-{DESCR}.md`, e.g. `105-mpq-reader.md`.

- `{PHASE}` — 1 to 10.
- `{ID}` — sequential within the phase, 01 onwards.
- `{DESCR}` — dash-separated short summary.

Sub-issues for a large piece of work append a letter: `105a-...`, `105b-...`.

## Indexed filename counter

`.file-index-counter` holds the current highest index across **all** indexed
files in the project (docs, source, tests, scripts). Reads before
creation, writes after. Currently at **11** (docs/000 … docs/011).
