# 101 — Build System

*Filled in part by the code from wow-chat-2026 (its 173k (the client in C)), 2026-10-07.*

## Current behavior

Built (2026-10-07, the move from wow-chat-2026): `scripts/build`, one
bash script calling the compiler directly (the owner's choice: no
CMake). It makes the client (`w-client`), the test program
(`w-client-test`, from `tests/044-test-client.c`) and the reader library
(`libwreaders.so` with a flattened header, `wreaders.h`, that LuaJIT's
FFI loads: checked by `scripts/libs/wreaders.lua`), into the RAM tier
(`tmp/tmp/build/`), in about 15 seconds. `--test` runs the tests;
`--asan` and `--tsan` build the address-checked and race-checked
clients. Each build's length goes to `tmp/shared-memory/build-times.log`,
with a warning past five minutes. Flags: C11, `-Wall -Wextra -Werror`.

Not as planned below: no CMake, no `--release` switch, no UBSan build,
raylib from `/usr/local` rather than `libs/` (fetching it into `libs/`
when absent is still to do), no `clean`/`format` scripts.

## Intended behavior

From a clean checkout, a new collaborator can run

```
scripts/build.sh
```

and end up with a `build/debug/custom-client` binary that prints its
version and exits 0. They can run `scripts/test.sh` and watch the (empty
for now) test suite pass. They can run `scripts/clean.sh` and the tree
returns to checkout state.

The build:

- Changed 2026-10-07: one bash script calling the compiler (the owner: "bash is easier for me. We can always call cmake inside of bash"), not CMake.
- Targets C11 with `-Wall -Wextra -Wpedantic -Werror`.
- Defaults to Debug + AddressSanitizer + UBSan; `--release` switches modes.
- Discovers OpenSSL and LuaJIT via `find_package`. Failure to find either
  prints an actionable apt/dnf/pacman install hint and exits non-zero.
- Builds vendored deps (`libs/raylib`, `libs/stormlib`, `libs/miniz`) as
  CMake subdirectories. raylib is vendored rather than system-discovered
  because distro packages lag the upstream release we want.
- Produces one executable: `custom-client`. Plus one test executable per
  subsystem under `build/<cfg>/tests/`.
- Also produces **`libwreaders.so`**, a shared library holding the reading
  layer (file access, MPQ chain, DBC, BLP; later M2) with a plain C interface
  and no raylib dependency. world-edit-to-execute loads it from Lua through
  LuaJIT's FFI (W client merge, 2026-09-23; see
  [docs/000](../docs/000-vision-summary.md)). The same object files link into
  the executable, so there is one implementation.

See [docs/008-build-system.md](../docs/008-build-system.md) for the
overall plan; this issue implements it.

## Suggested implementation steps

1. Write `CMakeLists.txt` at the project root: project declaration,
   options (`USE_ASAN`, `USE_UBSAN`, `BUILD_TESTS`), global compile flags.
2. Add `cmake/FindLuaJIT.cmake` (LuaJIT does not ship a CMake config).
3. Vendor raylib + StormLib + miniz under `libs/`. Add `add_subdirectory()`
   calls. raylib is pinned to a specific tagged release in
   `libs/raylib/VERSION`; the vendor script is `scripts/vendor.sh`.
4. Create `src/CMakeLists.txt`. For Phase 1 the executable is a stub
   `main()` that prints `custom-client v0.0.0` and exits.
5. Create `tests/CMakeLists.txt`. Empty for now — gets populated as
   subsystems land.
6. Write `scripts/build.sh`, `scripts/clean.sh`, `scripts/test.sh`,
   `scripts/run.sh`, `scripts/format.sh`. Each script: hard-coded `${DIR}`
   at the top, `--dir <path>` override, `--help`, everything relative to
   `${DIR}`. One bash command per line per CLAUDE.md.
7. Write `.gitignore` (build/, tmp/, assets/, `*.o`, IDE detritus).
8. Write `.clang-format` and `.editorconfig`.

## Acceptance

- `scripts/build.sh` succeeds on a clean checkout (assuming OpenSSL and
  LuaJIT are installed; raylib is vendored).
- `build/debug/custom-client --version` prints `custom-client 0.0.0` and
  exits 0.
- `scripts/test.sh` prints `0 tests / 0 failures` and exits 0.
- The build emits zero warnings.

## Stress markers

This is the kind of issue where it's tempting to over-engineer the
configuration system before any code exists to configure. Keep it small:
a working build that future issues can extend is worth more than a
"complete" build system that nothing uses yet.

## Related

- [docs/002-technology-stack.md](../docs/002-technology-stack.md)
- [docs/008-build-system.md](../docs/008-build-system.md)
- Blocks: 102, 103, 104, 105, 106, 107.
