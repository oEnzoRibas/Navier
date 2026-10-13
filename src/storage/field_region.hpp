#pragma once
#include <navier/format/file.hpp>
#include <navier/storage/field.hpp>
#include <chrono>
#include <random>

namespace navier::storage::detail {
constexpr std::uint64_t region_block_size = 65536;
constexpr std::size_t block_header_size = 128, max_block_directory = 16 * 1024 * 1024;

// Derived cache. Scientific .data remains unchanged. The table is tied to the
// dataset identity, immutable record generation, offset, byte length and CRC.
inline format::Bytes block_checksums(format::File& source,const std::filesystem::path& directory,
                                    const std::array<std::uint64_t,2>& identity,std::uint64_t generation,
                                    std::uint64_t offset,std::uint64_t bytes,std::uint32_t field_crc) {
    const auto blocks = bytes / region_block_size + (bytes % region_block_size != 0 ? 1U : 0U);
    const auto table_size = format::checked_mul(blocks,4);
    if (table_size > max_block_directory) throw std::length_error("Block checksum directory exceeds 16 MiB");
    const auto filename = std::to_string(offset)+"-"+std::to_string(bytes)+"-"+std::to_string(field_crc)+".nvc";
    const auto path = directory / filename;
    try {
        if (std::filesystem::is_regular_file(path)) {
            format::File cache(path);
            const auto head = cache.read(0,block_header_size);
            if (cache.size() != block_header_size + table_size ||
                !format::matches(head,0,"NVBLOCK1") || format::get(head,8,4)!=1 ||
                format::get(head,12,4)!=region_block_size ||
                format::get(head,16)!=identity[0] || format::get(head,24)!=identity[1] ||
                format::get(head,32)!=generation || format::get(head,40)!=offset ||
                format::get(head,48)!=bytes || format::get(head,56,4)!=field_crc ||
                format::get(head,64)!=blocks ||
                format::get(head,124,4)!=format::checksum(std::span(head).first(124)))
                throw std::runtime_error("Invalid derived block directory");
            auto table = cache.read(block_header_size,format::memory_size(table_size));
            if (format::get(head,72,4)!=format::checksum(table)) throw std::runtime_error("Block directory checksum mismatch");
            return table;
        }
    } catch (const std::exception&) { /* Damaged derived metadata is rebuildable. */ }

    format::Bytes table(format::memory_size(table_size));
    format::CRC32 full;
    std::array<std::byte,region_block_size> buffer{};
    for (std::uint64_t block=0;block<blocks;++block) {
        const auto relative = block*region_block_size;
        auto data = std::span(buffer).first(format::memory_size(std::min(region_block_size,bytes-relative)));
        source.read_into(format::checked_add(offset,relative),data);
        full.update(data);
        format::put(table,format::memory_size(block*4),format::checksum(data),4);
    }
    if (full.value()!=field_crc) throw std::runtime_error("Field checksum mismatch while building block directory");
    std::filesystem::create_directories(directory);
    format::Bytes head(block_header_size);
    format::magic(head,0,"NVBLOCK1"); format::put(head,8,1,4);format::put(head,12,region_block_size,4);
    format::put(head,16,identity[0]);format::put(head,24,identity[1]);format::put(head,32,generation);
    format::put(head,40,offset);format::put(head,48,bytes);format::put(head,56,field_crc,4);
    format::put(head,64,blocks);format::put(head,72,format::checksum(table),4);
    format::put(head,124,format::checksum(std::span(head).first(124)),4);
    auto temp = path;
    std::random_device random;
    temp += ".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(random());
    try {
        { std::ofstream create(temp,std::ios::binary);if(!create)throw std::runtime_error("Cannot create block directory"); }
        { format::File cache(temp);cache.write(0,head);cache.write(block_header_size,table);cache.flush(); }
        std::error_code error;
        std::filesystem::rename(temp,path,error);
        // Another reader may finish the identical immutable record first.
        if (error) {
            // Accept only an identical completed winner, not a stale/broken
            // file whose existence happens to mask a publication failure.
            bool identical=false;
            try { format::File winner(path);identical=winner.size()==head.size()+table.size() &&
                winner.read(0,head.size())==head && winner.read(head.size(),table.size())==table; }
            catch(const std::exception&) {}
            if(!identical)throw std::filesystem::filesystem_error("Publish block directory",temp,path,error);
            std::filesystem::remove(temp);
        }
    } catch (...) { std::error_code ignored;std::filesystem::remove(temp,ignored);throw; }
    return table;
}

inline Field read_region(format::File& source,const FieldDescriptor& descriptor,const FieldRegion& region,
                         std::uint64_t offset,std::uint32_t crc,std::uint64_t budget,
                         const std::filesystem::path& directory,const std::array<std::uint64_t,2>& identity,
                         std::uint64_t generation) {
    const auto rank = descriptor.dimensions.size();
    if(region.origin.size()!=rank||region.count.size()!=rank||region.stride.size()!=rank)
        throw std::invalid_argument("Region origin/count/stride must match field rank");
    std::uint64_t count=1,pitch=1;
    std::vector<std::uint64_t> pitches(rank),position(rank,0);
    for(std::size_t axis=0;axis<rank;++axis) {
        if(!region.count[axis]||!region.stride[axis])throw std::invalid_argument("Region count and stride must be positive");
        const auto last=format::checked_add(region.origin[axis],format::checked_mul(region.count[axis]-1,region.stride[axis]));
        if(last>=descriptor.dimensions[axis])throw std::out_of_range("Region exceeds field dimensions");
        count=format::checked_mul(count,region.count[axis]);pitches[axis]=pitch;
        pitch=format::checked_mul(pitch,descriptor.dimensions[axis]);
    }
    const std::uint64_t width=descriptor.scalar_type==ScalarType::float32?4:8;
    if(format::checked_mul(count,width)>budget)throw std::length_error("Region exceeds memory budget");
    const auto table=block_checksums(source,directory,identity,generation,offset,descriptor.byte_size(),crc);
    Field result;result.descriptor=descriptor;result.descriptor.dimensions=region.count;
    if(width==4)result.values=std::vector<float>(format::memory_size(count));
    else result.values=std::vector<double>(format::memory_size(count));
    format::Bytes buffer;
    auto loaded=std::numeric_limits<std::uint64_t>::max();
    for(std::uint64_t output=0;output<count;++output) {
        std::uint64_t index=0;
        for(std::size_t axis=0;axis<rank;++axis)
            index+=(region.origin[axis]+position[axis]*region.stride[axis])*pitches[axis];
        const auto relative=index*width,block=relative/region_block_size;
        if(block!=loaded) {
            const auto begin=block*region_block_size;
            buffer=source.read(format::checked_add(offset,begin),format::memory_size(std::min(region_block_size,descriptor.byte_size()-begin)));
            if(format::checksum(buffer)!=format::get(table,format::memory_size(block*4),4))
                throw std::runtime_error("Region block checksum mismatch");
            loaded=block;
        }
        const auto bits=format::get(buffer,format::memory_size(relative%region_block_size),format::memory_size(width));
        const auto at=format::memory_size(output);
        if(width==4)std::get<std::vector<float>>(result.values)[at]=std::bit_cast<float>(static_cast<std::uint32_t>(bits));
        else std::get<std::vector<double>>(result.values)[at]=std::bit_cast<double>(bits);
        for(std::size_t axis=0;axis<rank;++axis) {if(++position[axis]<region.count[axis])break;position[axis]=0;}
    }
    return result;
}
} // namespace navier::storage::detail
