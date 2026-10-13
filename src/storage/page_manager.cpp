#include <navier/storage/page_manager.hpp>
#include <navier/format/binary.hpp>
#include <limits>
#include <stdexcept>
namespace navier::storage {
namespace {
std::streamoff offset(PageId id){const auto v=format::checked_mul(id,page_size);if(v>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))throw std::runtime_error("Page address exceeds stream range");return static_cast<std::streamoff>(v);}
}
PageManager::PageManager(const std::filesystem::path& path,bool initialize){
    if(initialize){std::ofstream create(path,std::ios::binary|std::ios::trunc);if(!create)throw std::runtime_error("Cannot initialize index file");}
    file_.open(path,std::ios::binary|std::ios::in|std::ios::out);if(!file_)throw std::runtime_error("Cannot open index file");
    file_.seekg(0,std::ios::end);const auto size=file_.tellg();if(size<0||static_cast<std::uint64_t>(size)%page_size)throw std::runtime_error("Partial index page");count_=static_cast<std::uint64_t>(size)/page_size;
}
PageId PageManager::allocate_page(){const auto id=count_;Page zero{};file_.clear();file_.seekp(offset(id));file_.write(reinterpret_cast<const char*>(zero.data()),page_size);if(!file_)throw std::runtime_error("Cannot allocate page");++count_;return id;}
Page PageManager::read_page(PageId id){if(id>=count_)throw std::runtime_error("Invalid PageId");Page result{};file_.clear();file_.seekg(offset(id));file_.read(reinterpret_cast<char*>(result.data()),page_size);if(!file_)throw std::runtime_error("Short page read");return result;}
void PageManager::write_page(PageId id,const Page& page){if(id>=count_)throw std::runtime_error("Invalid PageId");file_.clear();file_.seekp(offset(id));file_.write(reinterpret_cast<const char*>(page.data()),page_size);if(!file_)throw std::runtime_error("Short page write");}
void PageManager::flush(){file_.flush();if(!file_)throw std::runtime_error("Index flush failed");}
}
