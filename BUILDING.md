# Building Chronos (C++)

The C++ hot path (`core/`) builds with CMake. This covers the toolchain added in **M0.2**.

## Prerequisites

| Tool | Version used | Notes |
|---|---|---|
| CMake | ≥ 3.24 (tested 4.3.1) | build configuration |
| C++ compiler | C++20 (tested AppleClang 16) | Clang or GCC |
| Ninja | tested 1.13.2 | recommended generator |

No third-party libraries are fetched during the build (the test harness is vendored), so a
configure + build works without network access.

## Quick start

```sh
cmake -S . -B build -G Ninja      # configure (Debug by default)
cmake --build build               # compile
ctest --test-dir build --output-on-failure   # run tests
```

## Build profiles

Select with `-DCMAKE_BUILD_TYPE=<profile>`:

| Profile | Flags | Use |
|---|---|---|
| `Debug` (default) | unoptimized, asserts on | development |
| `Release` | `-O3 -DNDEBUG` | general optimized build |
| `Benchmark` | `-O3 -DNDEBUG`, no `-march=native` | latency benchmarking (ADR-0002) — reproducible, host-neutral |

```sh
cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Benchmark
cmake --build build-bench
```

## Options

| Option | Default | Effect |
|---|---|---|
| `CHRONOS_WERROR` | `ON` | compiler warnings are errors |
| `CHRONOS_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer |

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCHRONOS_SANITIZE=ON
```

## What's here in M0.2

- `chronos_core` — placeholder library (`core/`) proving the toolchain links.
- `chronos_unit_tests` — unit-test executable registered with CTest.
- `tests/support/microtest.hpp` — vendored ~60-line, zero-dependency test harness.
- `chronos_warnings` / `chronos_options` — shared CMake interface targets for the warning set
  and language standard / sanitizers.

Real domain modules and a Python build (M0.3) arrive in later milestones.
