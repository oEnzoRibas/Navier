# Navier

Navier is an open-source experimental storage and indexing engine for numerical simulation states. This first version is also an Algorithms and Data Structures project: its central contribution is a generic, independently tested B+ Tree implemented from scratch.

## Motivation

Numerical simulations produce states over time. Checkpoints let applications explore earlier states without rerunning the solver. Exact step lookup and temporal interval queries are natural operations. A B+ Tree provides logarithmic lookup and efficient traversal of ordered, linked leaves.

Navier is an educational foundation, with no claim of superiority over HDF5, NetCDF, or mature HPC storage tools.

## Current scope

- Generic in-memory B+ Tree: insertion, replacement, exact lookup, inclusive range queries, linked leaves, and structural validation.
- Append-only binary checkpoint file containing step, time, and one `std::vector<double>` field.
- RAM index rebuilt by scanning record headers on reopening a file.
- Synthetic checkpoint demo, two external 2D simulation producers, GoogleTest suite, and linear-index comparison benchmark.

Future milestones include a persistent page-based B+ Tree, buffer pool, scientific metadata indexes, advanced solver integrations, an interactive viewer, compression, and asynchronous checkpointing. None is implemented in this MVP.

## Build and test

Requires CMake 3.24+, a C++23-capable compiler, and Ninja (or another CMake generator). GoogleTest is the only library dependency of the tests. CMake first looks for an installed GTest package; otherwise it downloads GoogleTest 1.17.0 at a pinned commit using Git. The initial download requires network access.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/navier_demo ./build/sim.data
```

On Windows, executable filenames have the `.exe` suffix. With MSVC, run from a Developer PowerShell/Command Prompt. With MinGW, ensure its `bin` directory is in `PATH`. Multi-config generators also require `--config Debug` for build and `-C Debug` for CTest.

Options (defaults shown):

| Option | Default | Purpose |
| --- | --- | --- |
| `NAVIER_BUILD_TESTS` | `ON` | GoogleTest unit tests |
| `NAVIER_BUILD_EXAMPLES` | `ON` | Synthetic demo |
| `NAVIER_BUILD_BENCHMARKS` | `ON` | Lookup benchmark |
| `NAVIER_BUILD_SIMULATIONS` | `ON` | External heat/fluid 2D producers |
| `NAVIER_WARNINGS_AS_ERRORS` | `ON` | Strict project warnings |
| `NAVIER_ENABLE_ASAN` | `OFF` | AddressSanitizer |
| `NAVIER_ENABLE_UBSAN` | `OFF` | UndefinedBehaviorSanitizer (GCC/Clang) |

For a supported GCC/Clang toolchain:

```sh
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DNAVIER_ENABLE_ASAN=ON -DNAVIER_ENABLE_UBSAN=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

MSVC supports the ASan option; UBSan is unavailable with MSVC. Sanitizer runtimes must be installed with the compiler. For offline builds without an installed GTest package, pass `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest` to use a local source checkout. Tests can also be disabled with `-DNAVIER_BUILD_TESTS=OFF`.

## Explore files directly with Navier

The `navier_cli` build target produces the `navier` executable (`navier.exe` on Windows). It opens an existing checkpoint file, rebuilds the B+ Tree index, and exposes the same storage API directly from the terminal:

```powershell
.\build-msvc\Release\navier.exe info heat2d.data
.\build-msvc\Release\navier.exe get fluid2d.data 400
.\build-msvc\Release\navier.exe range heat2d.data 200 300
```

`info` prints file size, unique checkpoint count, step bounds, and the first ten records. `get` loads one payload and prints its length plus a preview of the first eight values. `range` lists matching metadata and record locations with inclusive bounds. `--help` prints usage. Missing files are rejected without creating them. Missing steps return exit code 2; other errors return 1. The current storage API requires write access when opening its stream, although these CLI commands do not modify records. Disable this executable with `-DNAVIER_BUILD_CLI=OFF`.

This terminal explorer is the direct user interface to Navier's storage/indexing engine. The optional plot script below provides static images; an interactive graphical viewer remains a future milestone.

## Run the two 2D simulations

Two small external producers demonstrate real numerical fields while keeping all physics outside the storage library:

- `navier_heat2d`: explicit five-point heat diffusion, zero-temperature boundaries, a smooth sine-mode initial field, and a stable timestep.
- `navier_fluid2d`: BGK LBM D2Q9, a low-Mach Taylor-Green vortex, and periodic boundaries in lattice units.

Both default to a 64 by 64 grid, 400 steps, and a checkpoint every 20 steps, including initial and final states. Each uses a separate binary file and JSON sidecar describing grid, field layout, boundaries, and timestep. The heat field is one row-major temperature plane. The fluid field is three row-major planes: density, x velocity, y velocity. Physical time units differ between these examples.

```sh
./build/navier_heat2d heat2d.data 64 400 20
./build/navier_fluid2d fluid2d.data 64 400 20
```

Arguments are `[data-file] [grid-side] [steps] [save-every]`. Grid sides are limited to 8 through 512. Producers reject an existing output path to avoid mixing independent runs; use a new filename for each run. These are reproducible educational simulations with fixed initial conditions, not general-purpose configurable CFD solvers.

A Windows/MSVC build matching the verified local toolchain is:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Release --parallel 1
ctest --test-dir build-msvc -C Release --output-on-failure
.\build-msvc\Release\navier_heat2d.exe heat2d.data 64 400 20
.\build-msvc\Release\navier_fluid2d.exe fluid2d.data 64 400 20
```

An optional static plotting script reads the saved checkpoint files, compares initial/final states with shared color scales, and prints basic numerical diagnostics:

```sh
python -m pip install numpy matplotlib
python examples/plot_2d.py heat2d.data fluid2d.data --output simulations.png
```

Python plotting packages are optional and do not affect the C++ build. The script is a format reader, not a Python binding. Navier has no interactive GUI yet. See [running simulations](docs/running-simulations.md) for Portuguese usage instructions and numerical assumptions.

## Public API

Link your target against `navier::navier` and include `<navier/navier.hpp>`:

```cpp
#include <navier/navier.hpp>

std::vector<double> field{1.0, 2.0, 3.0};
navier::storage::CheckpointStore store("sim.data");
store.save(100, 0.1, field);
auto checkpoint = store.load(100);
auto metadata = store.range(0, 200); // Inclusive step bounds.
auto location = store.location(100); // optional offset + record size.
```

`load()` throws `std::out_of_range` for an absent step. File and format errors throw exceptions. Parent directories must already exist. New files receive a format signature; existing empty, unsupported, or truncated files are rejected without truncating them.

Duplicate keys replace values. The store appends a new record for a repeated step and points its index at the latest record. Reopening preserves this policy. Older records remain on disk; compaction is outside the current scope.

The generic tree is independent of storage:

```cpp
navier::index::BPlusTree<int, std::string, 4> index;
bool inserted = index.insert(10, "checkpoint"); // true for a new key
index.insert(10, "replacement");              // false; size is unchanged
const std::string* value = index.find(10);    // nullptr if absent
auto entries = index.range(0, 20);           // ordered key/value pairs
bool valid = index.validate();
```

`Order` is the maximum number of internal children, and must be at least 3. Keys and values must be copyable; `Compare` must be a strict weak ordering. Comparator equivalence defines duplicate keys. Range bounds follow that comparator; reversed bounds return an empty result. Pointers returned by `find()` must not be retained across insertions. Trees cannot be copied or moved in this version.

## Tests and invariants

Tests cover empty/single trees, duplicate replacement, first leaf/root split, internal splits and multiple levels, missing keys, ordered and random insertions, inclusive and empty ranges, custom comparators, unsigned boundary keys, and non-default-constructible values. Orders 3, 4, 5, 32, and 64 exercise odd and even capacities. Random tests use a fixed seed and compare against `std::map` exclusively as a test oracle.

`validate()` checks strict key ordering, occupancy bounds, equal leaf depth, key/value alignment, internal child counts, exact right-subtree-minimum separators, disjoint ordered child ranges, complete leaf links, and total element count. See [architecture](docs/architecture.md) for split rules and file layout.

Storage tests cover metadata/payload round trips, duplicate steps, reopening and continued appends, ranges across index leaves, empty fields, binary64 bit preservation, little-endian encoding, invalid signatures, oversized counts, and truncation boundaries.

The two producer solvers are also tested: heat stability and the discrete/continuum sine-mode solutions, fluid rest equilibrium, periodic mass/momentum conservation, kinetic-energy decay, and parameter validation.

## Benchmark

Build in Release mode for timings:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/navier_benchmark 1000 1000
./build-release/navier_benchmark > benchmark.csv
```

Without arguments the datasets are 1,000, 10,000, 100,000, and 1,000,000 elements, each with 1,000 lookups. Optional arguments select element and lookup counts. Output is CSV: elements, tree height, lookups, nanoseconds per lookup for both indexes, mean comparisons for both indexes, and checksum.

Both indexes contain the same shuffled keys and use the same pre-generated queries, including misses, with a fixed seed. Construction, validation, and query generation are excluded from timing. Each index receives a warm-up pass. Checksums are printed and compared to retain observable lookup work. Linear comparisons count equality checks; B+ Tree comparisons count ordering predicate calls, so these counts describe different operations. Instrumentation is included in both timings. A single sample is a smoke benchmark, not a statistical performance study; repeat runs and report compiler, configuration, hardware, and workload when interpreting results. No benchmark numbers are claimed here.

## Limitations and evolution

The tree and store are single-threaded. The store is not transactional, offers no crash recovery or checksums, and requires exclusive access to its file. `flush()` checks stream errors but does not guarantee power-loss durability. A failed write can leave a truncated tail, which is rejected on reopening. Insertion assumes successful allocations and nonthrowing element/comparator operations; there is no strong exception guarantee for allocation failures or throwing user types. After such an insertion failure, discard the tree/store and reopen the file when valid.

The B+ Tree is **not persisted**: reopening scans record headers, reconstructs the index, and skips payload bytes. This is a linear-time startup step. Range metadata queries avoid loading numerical fields. The format uses IEEE-754 binary64 and little-endian integers; arbitrary file contents are not authenticated. Large range results are materialized in memory.

Future evolution replaces the store's internal index with a page-based persistent tree without exposing those details to producers. No solver-specific or physics-specific abstractions are part of the index. See [architecture](docs/architecture.md).

## License

MIT. See [LICENSE](LICENSE).
