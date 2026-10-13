#pragma once
#include <navier/index/bplus_tree.hpp>
#include <navier/storage/checkpoint.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>

namespace navier::storage {
// Single-threaded append-only store. Opening a file rebuilds the RAM index.
class CheckpointStore {
public:
    explicit CheckpointStore(const std::filesystem::path& path);
    CheckpointStore(const CheckpointStore&) = delete;
    CheckpointStore& operator=(const CheckpointStore&) = delete;
    void save(std::uint64_t step, double time, std::span<const double> values);
    [[nodiscard]] Checkpoint load(std::uint64_t step, std::uint64_t max_bytes = 256ULL * 1024 * 1024);
    [[nodiscard]] std::vector<CheckpointMetadata> range(std::uint64_t begin, std::uint64_t end);
    [[nodiscard]] std::optional<CheckpointLocation> location(std::uint64_t step) const;
    [[nodiscard]] std::size_t size() const noexcept { return index_.size(); }
private:
    void rebuild_index();
    void seek_read(std::uint64_t offset);
    std::fstream file_;
    index::BPlusTree<std::uint64_t, CheckpointLocation> index_;
    std::uint64_t end_offset_ = 0;
};
} // namespace navier::storage
