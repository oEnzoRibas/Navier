#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace navier::storage {
using PageId=std::uint64_t;
inline constexpr std::size_t page_size=4096;
using Page=std::array<std::byte,page_size>;
struct BufferStatistics {std::uint64_t hits=0,misses=0,evictions=0,dirty_writes=0;std::size_t cached=0,pinned=0;};
} // namespace navier::storage
