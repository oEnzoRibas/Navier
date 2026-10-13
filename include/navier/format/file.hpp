#pragma once
#include <navier/format/binary.hpp>
#include <filesystem>
#include <fstream>
namespace navier::format {
class File {
    std::fstream stream_;std::filesystem::path path_;
    static std::streamoff offset(std::uint64_t n){if(n>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))throw std::runtime_error("File offset exceeds stream range");return static_cast<std::streamoff>(n);}
public:
    std::uint64_t bytes_read=0,bytes_written=0;
    explicit File(const std::filesystem::path& path):path_(path){open();}
    void open(){stream_.open(path_,std::ios::binary|std::ios::in|std::ios::out);if(!stream_)throw std::runtime_error("Cannot open dataset file");}
    [[nodiscard]] std::uint64_t size()const{return std::filesystem::file_size(path_);}
    void read_into(std::uint64_t at,std::span<std::byte> bytes){const auto length=size();if(at>length||bytes.size()>length-at)throw std::runtime_error("Read outside dataset bounds");stream_.clear();stream_.seekg(offset(at));stream_.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!stream_)throw std::runtime_error("Short dataset read");bytes_read+=bytes.size();}
    [[nodiscard]] Bytes read(std::uint64_t at,std::size_t count){const auto length=size();if(at>length||count>length-at)throw std::runtime_error("Read outside dataset bounds");Bytes bytes(count);read_into(at,bytes);return bytes;}
    void write(std::uint64_t at,std::span<const std::byte> bytes){(void)offset(checked_add(at,bytes.size()));stream_.clear();stream_.seekp(offset(at));stream_.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!stream_)throw std::runtime_error("Short dataset write");bytes_written+=bytes.size();}
    void flush(){stream_.flush();if(!stream_)throw std::runtime_error("Dataset flush failed");}
    void truncate(std::uint64_t end){flush();stream_.close();std::filesystem::resize_file(path_,end);open();}
};
}
