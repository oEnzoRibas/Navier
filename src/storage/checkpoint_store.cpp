#include <navier/storage/checkpoint_store.hpp>
#include <array>
#include <navier/format/binary.hpp>
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace navier::storage {
namespace {
constexpr std::array<char, 8> magic{'N', 'A', 'V', 'I', 'E', 'R', '0', '1'};
constexpr std::uint64_t header_size = 24;
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "Checkpoint format requires IEEE-754 binary64");

void write_u64(std::ostream& out, std::uint64_t value) {
    std::array<unsigned char, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<unsigned char>((value >> (8 * i)) & 0xffU);
    out.write(reinterpret_cast<const char*>(bytes.data()), 8);
    if (!out) throw std::runtime_error("Cannot write checkpoint file");
}
std::uint64_t read_u64(std::istream& in) {
    std::array<unsigned char, 8> bytes{};
    in.read(reinterpret_cast<char*>(bytes.data()), 8);
    if (!in) throw std::runtime_error("Truncated or unreadable checkpoint file");
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        value |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
    return value;
}
std::streamoff stream_offset(std::uint64_t value) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        throw std::overflow_error("Checkpoint file exceeds stream address range");
    return static_cast<std::streamoff>(value);
}
} // namespace

CheckpointStore::CheckpointStore(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::ofstream create(path, std::ios::binary);
        create.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        create.close();
        if (!create) throw std::runtime_error("Cannot create checkpoint file");
    }
    file_.open(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_) throw std::runtime_error("Cannot open checkpoint file");
    rebuild_index();
}

void CheckpointStore::seek_read(std::uint64_t offset) {
    file_.seekg(stream_offset(offset), std::ios::beg);
    if (!file_) throw std::runtime_error("Cannot seek checkpoint file");
}

void CheckpointStore::rebuild_index() {
    file_.seekg(0, std::ios::end);
    const auto end = file_.tellg();
    if (end < 0) throw std::runtime_error("Cannot determine checkpoint file size");
    end_offset_ = static_cast<std::uint64_t>(end);
    seek_read(0);
    std::array<char, 8> signature{};
    file_.read(signature.data(), static_cast<std::streamsize>(signature.size()));
    if (!file_ || signature != magic) throw std::runtime_error("Invalid Navier file signature/version");
    std::uint64_t offset = magic.size();
    while (offset < end_offset_) {
        if (end_offset_ - offset < header_size) throw std::runtime_error("Truncated checkpoint header");
        seek_read(offset);
        const auto step = read_u64(file_);
        (void)read_u64(file_); // Time bits are metadata, not part of the index.
        const auto count = read_u64(file_);
        if (count > (end_offset_ - offset - header_size) / 8)
            throw std::runtime_error("Truncated checkpoint payload or invalid value count");
        const auto size = header_size + count * 8;
        index_.insert(step, {offset, size}); // Last record wins for repeated steps.
        offset += size;
    }
}

void CheckpointStore::save(std::uint64_t step, double time, std::span<const double> values) {
    const auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    if (end_offset_ > max_offset - header_size ||
        values.size() > (max_offset - end_offset_ - header_size) / 8)
        throw std::overflow_error("Checkpoint payload is too large");
    const auto record_size = header_size + static_cast<std::uint64_t>(values.size()) * 8;
    file_.seekp(stream_offset(end_offset_), std::ios::beg);
    if (!file_) throw std::runtime_error("Cannot seek checkpoint file for writing");
    write_u64(file_, step);
    write_u64(file_, std::bit_cast<std::uint64_t>(time));
    write_u64(file_, static_cast<std::uint64_t>(values.size()));
    std::array<std::byte, 65536> buffer{};
    for (std::size_t base=0;base<values.size();) {
        const auto count=std::min(values.size()-base,buffer.size()/8);
        for(std::size_t i=0;i<count;++i) format::put(buffer,i*8,std::bit_cast<std::uint64_t>(values[base+i]));
        file_.write(reinterpret_cast<const char*>(buffer.data()),static_cast<std::streamsize>(count*8));
        if(!file_)throw std::runtime_error("Cannot write checkpoint payload block");
        base+=count;
    }
    file_.flush();
    if (!file_) throw std::runtime_error("Cannot flush checkpoint file");
    const auto offset = end_offset_;
    end_offset_ += record_size;
    index_.insert(step, {offset, record_size});
}

std::optional<CheckpointLocation> CheckpointStore::location(std::uint64_t step) const {
    if (const auto* found = index_.find(step)) return *found;
    return std::nullopt;
}

Checkpoint CheckpointStore::load(std::uint64_t step, std::uint64_t max_bytes) {
    const auto where = location(step);
    if (!where) throw std::out_of_range("Checkpoint step not found");
    seek_read(where->offset);
    Checkpoint result;
    result.metadata.step = read_u64(file_);
    result.metadata.time = std::bit_cast<double>(read_u64(file_));
    const auto count = read_u64(file_);
    if (result.metadata.step != step || where->size < header_size || count != (where->size - header_size) / 8)
        throw std::runtime_error("Checkpoint header changed since indexing");
    if (count > max_bytes / 8) throw std::length_error("Legacy checkpoint exceeds memory budget; use NAVIER02 field-selective storage");
    if (count > result.values.max_size()) throw std::length_error("Checkpoint payload exceeds vector capacity");
    result.values.resize(static_cast<std::size_t>(count));
    std::array<std::byte,65536> buffer{};
    for(std::size_t base=0;base<result.values.size();) {
        const auto n=std::min(result.values.size()-base,buffer.size()/8);
        file_.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(n*8));
        if(!file_)throw std::runtime_error("Short checkpoint payload block");
        for(std::size_t i=0;i<n;++i)result.values[base+i]=std::bit_cast<double>(format::get(buffer,i*8));
        base+=n;
    }
    return result;
}

std::vector<CheckpointMetadata> CheckpointStore::range(std::uint64_t begin, std::uint64_t end) {
    std::vector<CheckpointMetadata> result;
    for (const auto& [step, where] : index_.range(begin, end)) {
        seek_read(where.offset);
        const auto stored_step = read_u64(file_);
        const auto time = std::bit_cast<double>(read_u64(file_));
        if (stored_step != step) throw std::runtime_error("Checkpoint header changed since indexing");
        result.push_back({step, time});
    }
    return result;
}
} // namespace navier::storage
