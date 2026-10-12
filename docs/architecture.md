# Architecture

## MVP boundaries

```text
External solver / producer
           |
     Navier public API
           |
     CheckpointStore
       /        \
 binary file   generic B+ Tree in RAM
               step -> CheckpointLocation
```

The producer supplies step, time, and one span of doubles. `CheckpointStore` owns the stream and index and handles serialization. The B+ Tree owns nodes through `std::unique_ptr`, knows only Key/Value/Compare, and has no physics or file-format dependency. Leaf `next` pointers observe nodes owned by the root/child hierarchy; they never own or delete nodes.

## Tree representation and invariants

Order M means at most M internal children and M-1 keys per node. The root starts as an empty leaf. Height counts levels, so even an empty tree has height 1.

- All node keys are strictly ordered by Compare. Equivalent keys replace the associated value without changing cardinality.
- Leaves contain equally many keys and values and no children. Non-root leaves contain at least floor(M/2) keys.
- Internal nodes contain k keys and k+1 owning children, no values, and no leaf-link pointer. Non-root internal nodes contain at least ceil(M/2)-1 keys; an internal root has at least one key.
- An internal separator is exactly the smallest key of its right subtree. Keys in the left adjacent subtree precede that separator. Routing uses `upper_bound`, so an equal key goes right.
- Every leaf has the same depth. The forward leaf chain matches depth-first child order and ends in null.
- The element count equals the sum of leaf occupancies; copied internal separators do not count as elements.

`validate()` recursively checks these conditions, including actual child minima/maxima and exact leaf links. It is a diagnostic O(n) operation; lookups do not invoke it. Tests validate every insertion of 5,000-key ordered, reversed, and shuffled datasets for five orders, then compare lookup/range results against a reference map.

## Insertion and splits

Insertion descends using separators and inserts in a leaf with `lower_bound`. A leaf overflow has M keys and splits at floor(M/2), retaining the lower part and placing the upper part into a new sibling. The new right leaf inherits the old next link, and the old leaf links to it. The right minimum is **copied** up; all actual values remain in leaves.

A child split returns its separator and an owning right sibling to the recursive caller. The caller inserts them next to the split child. An internal overflow has M keys and M+1 children. Its middle separator is **promoted**, with the lower keys/children kept in the original node and the higher keys/children moved to the right sibling. Unlike a leaf split, the promoted key is removed from both internal halves. Root overflow creates a new internal root with two children. Height increases only through this step.

No deletion or parent pointers are needed. Without deletion, a non-leftmost subtree minimum cannot decrease through insertion: a smaller key is routed to the preceding subtree. Therefore, ordinary insertions and value replacements do not require separator updates; splits provide new separators.

For fixed order, lookup and insertion traverse O(log n) levels; vector shifts during insertion cost O(M) per affected node. Range search descends once and follows leaf links, with O(log n + r) output work (plus leaf traversal overhead). Nodes use reserved vectors for clarity, without page packing or premature optimization.

## Binary file format, version 01

The file starts with the eight ASCII bytes `NAVIER01`. Every following record is:

| Relative offset | Width | Encoding |
| --- | --- | --- |
| 0 | 8 bytes | step, unsigned little-endian |
| 8 | 8 bytes | time, IEEE-754 binary64 bits, little-endian |
| 16 | 8 bytes | numerical value count, unsigned little-endian |
| 24 | count * 8 bytes | binary64 value bits, little-endian |

No C++ struct is dumped to disk: padding and host endian order never enter the format. Compilation requires IEEE-754 64-bit doubles. NaN, infinity, and signed zero bits are preserved, with no application-level validation of numerical values.

`CheckpointLocation.offset` points to a record header, and `size` includes its 24-byte header and payload, excluding the file signature. `save()` appends a complete record, flushes/checks the stream, then updates the RAM index. A duplicate step appends again; latest successful record wins. Parent directories must exist. Creation never intentionally truncates existing files.

Opening scans the file from offset 8, checks available header/payload lengths before arithmetic or allocation, and inserts each step/location into the B+ Tree. Numerical payloads are skipped while rebuilding. Unsupported signatures, incomplete headers, and incomplete/oversized payloads fail construction. Bounds are checked against stream addressability and vector capacity before reading payloads. Exact load materializes the field; a metadata range visits only selected record headers in step order, even when record offsets are out of order.

The store assumes exclusive file access. Multiple writers, external modification, transactional writes, checksums, tail repair, and fsync/power-loss guarantees are outside scope. A failed append can leave an invalid tail. The index has no strong exception guarantee for allocator failures or throwing generic types; callers must discard a failed tree/store. No on-disk tree nodes are written.

## Targets and verification

- `navier`: storage library, exposing generic headers through `navier::navier`.
- `navier_cli`: builds executable `navier` with `info`, `get`, and inclusive `range` commands over existing files.
- `navier_tests`: GoogleTest tests for tree and storage.
- `navier_demo`: synthetic fields; no numerical solver.
- `navier_benchmark`: linear scan versus B+ Tree exact lookup with reproducible queries.
- `navier_heat2d` / `navier_fluid2d`: external heat-diffusion and periodic LBM producers, saving through the public API.
- `examples/plot_2d.py`: optional offline static plots; no GUI or Python binding.

The two numerical producers share only example-level CLI/sidecar helpers. Their solver implementation lives under `examples/solvers/`; no physical fields, lattice directions, or numerical stencils enter the generic index or storage library. The binary store still accepts a single vector: field names and dimensions are described by an external JSON sidecar.

Strict project warnings are errors by default. Sanitizer switches are configurable and toolchain-dependent. Test dependencies do not become public library dependencies. GoogleTest can come from an installed package or a pinned source checkout.

## Future architecture (not implemented)

```text
Solver
  |
Navier API
  +--- Checkpoint Store
  +--- Temporal Index
  +--- Metric Indexes
  +--- Query Engine
  +--- Viewer
```

A later storage implementation may substitute a persistent page-based B+ Tree behind `CheckpointStore`; public operations do not expose nodes or split rules. Page management and a buffer pool belong to that future milestone. Additional scientific metadata indexes, compression, visualization, and asynchronous checkpointing should be introduced only with concrete use cases and tests.

Possible producers include CFD, shallow-water, wave, heat-transfer, electromagnetic, FEM, and particle solvers. Their numerical schemes and domain fields remain external to the generic index. This MVP intentionally introduces no abstract factories, solver hierarchy, query framework, or other abstractions for those possibilities.
