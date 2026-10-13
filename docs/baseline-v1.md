# Baseline before storage-engine v1

Inspected on 2026-10-12, branch feat/navier-storage-engine-v1. Existing changes to viewer playback are retained separately; not part of the storage redesign.

- Generic RAM B+ Tree: Order=32, unique_ptr node ownership, linked leaves, upper_bound routing, lower_bound leaf search, duplicate replacement, splits and comprehensive validate(). No deletion/clear/persistent pages.
- CheckpointStore: NAVIER01, 8-byte magic, 24-byte LE headers, flat binary64 array; writes values individually; flush then RAM index update. Reopens by scanning headers. No field schema, payload checksum, completion marker, recovery or fsync guarantee. Single writer/thread required.
- Public core: save/load/range/location, legacy metadata step/time. CLI info/get/range and HTML exporter use this API.
- CMake C++23, MSVC /W4 /WX, GoogleTest, benchmark and external educational solvers. Existing tests cover RAM tree orders 3/4/5/32/64, legacy bit-exact serialization/truncation and heat/LBM/cavity/shear.
- Examples: heat diffusion 2D, D2Q9 fluid, multigrid cavity projection, shear, D3Q19 ABC flow, Maxwell 2D/3D, Lorenz phase-space density and Wave3D. Sidecars describe planar field order. All physics stays under examples.
- React strict TS/Vite/Three.js viewer: NAVIER01 compatibility parser, File.slice/verified HTTP Range, byte LRU, workers, bounded prepared-frame prefetch, GPU scalar/slices/cloud/volume/vectors/streamlines, timeline and metadata metrics. NavierDataSource is independent of core/file layout; no C++ backend yet.

Evolution will use a separate versioned DatasetStore API, preserving CheckpointStore and existing datasets unchanged. No legacy file will be silently upgraded. Milestones and actual guarantees are documented and tested before extending demos.
