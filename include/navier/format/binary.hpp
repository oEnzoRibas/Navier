#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace navier::format {
using Bytes = std::vector<std::byte>;
inline std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) throw std::runtime_error("Integer offset overflow");
    return a + b;
}
inline std::uint64_t checked_mul(std::uint64_t a, std::uint64_t b) {
    if (a && b > std::numeric_limits<std::uint64_t>::max() / a) throw std::runtime_error("Integer size overflow");
    return a * b;
}
inline std::size_t memory_size(std::uint64_t n) {
    if (n > std::numeric_limits<std::size_t>::max()) throw std::runtime_error("Size exceeds addressable memory");
    return static_cast<std::size_t>(n);
}
inline void bounds(std::span<const std::byte> bytes, std::size_t at, std::size_t size) {
    if (at > bytes.size() || size > bytes.size() - at) throw std::runtime_error("Truncated binary structure");
}
inline std::uint64_t get(std::span<const std::byte> bytes, std::size_t at, std::size_t width=8) {
    if (width > 8) throw std::logic_error("Invalid integer width");
    bounds(bytes, at, width); std::uint64_t v=0;
    for (std::size_t i=0;i<width;++i) v |= std::uint64_t(std::to_integer<unsigned char>(bytes[at+i])) << (i*8);
    return v;
}
inline void put(std::span<std::byte> bytes, std::size_t at, std::uint64_t value, std::size_t width=8) {
    if(width>8)throw std::logic_error("Invalid integer width");
    bounds(bytes,at,width);
    for(std::size_t i=0;i<width;++i) bytes[at+i]=static_cast<std::byte>((value >> (i*8)) & 255U);
}
inline void magic(std::span<std::byte> bytes, std::size_t at, std::string_view text) {
    bounds(bytes,at,text.size());
    for(std::size_t i=0;i<text.size();++i) bytes[at+i]=static_cast<std::byte>(text[i]);
}
inline bool matches(std::span<const std::byte> bytes, std::size_t at, std::string_view text) {
    bounds(bytes,at,text.size());
    for(std::size_t i=0;i<text.size();++i) if(bytes[at+i]!=static_cast<std::byte>(text[i])) return false;
    return true;
}
inline constexpr auto crc_table=[] {
    std::array<std::uint32_t,256> table{};
    for(std::uint32_t i=0;i<256;++i){auto c=i;for(int bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320U:0U);table[i]=c;}
    return table;
}();
class CRC32 {
    std::uint32_t state_=0xffffffffU;
public:
    void update(std::span<const std::byte> bytes){for(const auto b:bytes)state_=crc_table[(state_^std::to_integer<unsigned char>(b))&255U]^(state_>>8);}
    [[nodiscard]] std::uint32_t value()const{return state_^0xffffffffU;}
};
inline std::uint32_t checksum(std::span<const std::byte> bytes){CRC32 crc;crc.update(bytes);return crc.value();}
class Encoder {
public:
    Bytes bytes;
    void number(std::uint64_t value,std::size_t width=8){const auto at=bytes.size();bytes.resize(at+width);put(bytes,at,value,width);}
    void text(std::string_view value){if(value.size()>255)throw std::invalid_argument("Names/units must fit 255 bytes");number(value.size(),2);for(char c:value)bytes.push_back(static_cast<std::byte>(c));}
};
class Decoder {
    std::span<const std::byte> bytes_;std::size_t at_=0;
public:
    explicit Decoder(std::span<const std::byte> bytes):bytes_(bytes){}
    std::uint64_t number(std::size_t width=8){const auto v=get(bytes_,at_,width);at_+=width;return v;}
    std::string text(){const auto size=memory_size(number(2));if(size>255)throw std::runtime_error("Name length exceeds limit");bounds(bytes_,at_,size);std::string s; s.reserve(size);for(std::size_t i=0;i<size;++i)s.push_back(static_cast<char>(std::to_integer<unsigned char>(bytes_[at_++])));return s;}
    [[nodiscard]] bool done()const{return at_==bytes_.size();}
};
} // namespace navier::format
