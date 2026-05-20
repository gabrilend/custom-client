# 008 — Build System

CMake ≥ 3.20, wrapped by shell scripts under `scripts/`. Per CLAUDE.md, all
scripts have a hard-coded `${DIR}` at the top and accept a path argument
to override it; all paths inside the scripts are relative to `${DIR}`.

## Entry points

| Script               | Purpose                                                   |
|----------------------|-----------------------------------------------------------|
| `scripts/build.sh`   | Configure + build. Defaults to debug + asan.              |
| `scripts/run.sh`     | Build then launch the client with a dev config.           |
| `scripts/test.sh`    | Build then run the test binaries.                         |
| `scripts/clean.sh`   | Remove the build tree.                                    |
| `scripts/format.sh`  | `clang-format -i` over `src/`. Style: project `.clang-format`. |

All five take `--dir <path>` to override `${DIR}`, plus pass-through args
to the underlying tool.

## Output layout

```
${DIR}/
  build/
    debug/        # cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
    release/      # cmake -DCMAKE_BUILD_TYPE=Release
    test/         # built test binaries
  tmp/            # symlink → /tmp/azc-custom-client/ (RAM-backed logs, dumps)
```

We never write logs or intermediate artifacts to `${DIR}/` outside
`build/` and `tmp/`. Git ignores both.

## CMake structure

- Top-level `CMakeLists.txt` declares the project, options, and pulls in
  `cmake/` modules.
- `src/CMakeLists.txt` builds the client executable.
- `src/<subsystem>/CMakeLists.txt` builds each subsystem as a static
  library, linked into the executable. One static lib per subsystem keeps
  link-order issues local.
- `tests/CMakeLists.txt` builds the test binaries. One executable per
  subsystem.

## Dependency strategy

- **Vendored** (in `libs/`, built from source as subdirectories): StormLib,
  miniz, glad2, our math, our XML parser.
- **System-provided** (found via `find_package`): SDL3, OpenSSL, LuaJIT.
  Distros ship them; bundling is more pain than gain.

## Compile flags (all targets)

```
-std=c11 -Wall -Wextra -Wpedantic -Werror
-Wno-unused-parameter   # ubiquitous in event handlers
-Wno-missing-field-initializers
```

Sanitizer-debug additionally gets `-fsanitize=address,undefined -g3 -O1`.

## Test harness

Each subsystem's `tests/` directory is its own tiny test executable
linking against the subsystem's static lib. Convention: `assert_eq`,
`assert_true`, exit 0 on success, exit non-zero with a printed line on
first failure. No framework — keeps the test signal-to-noise high and the
build dependency-free.

## CI shape (future)

Three jobs in a matrix:

1. Debug + asan + ubsan, run tests.
2. Release, run tests.
3. clang-format check.

CI not configured yet — capture the shape now so the first config commit
has somewhere to point.

## Related

- [009-roadmap.md](./009-roadmap.md) — the build system itself is the
  first Phase 1 issue.
- [002-technology-stack.md](./002-technology-stack.md) — what's being
  built.
