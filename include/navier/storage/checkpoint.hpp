#pragma once
#include <cstdint>
#include <vector>

namespace navier::storage {
struct CheckpointLocation {
    std::uint64_t offset;
    std::uint64_t size;
    bool operator==(const CheckpointLocation&) const = default;
};
struct CheckpointMetadata {
    std::uint64_t step;
    double time;
    bool operator==(const CheckpointMetadata&) const = default;
};
struct Checkpoint {
    CheckpointMetadata metadata;
    std::vector<double> values;
};
} // namespace navier::storage
