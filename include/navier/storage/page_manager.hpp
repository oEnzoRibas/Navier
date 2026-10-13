#pragma once
#include <navier/storage/page.hpp>
#include <filesystem>
#include <fstream>
namespace navier::storage {
class PageManager {
    std::fstream file_;PageId count_=0;
public:
    explicit PageManager(const std::filesystem::path& path,bool initialize=false);
    PageManager(const PageManager&)=delete;PageManager& operator=(const PageManager&)=delete;
    [[nodiscard]] PageId allocate_page();
    [[nodiscard]] Page read_page(PageId id);
    void write_page(PageId id,const Page& page);
    void flush_page(PageId id,const Page& page){write_page(id,page);flush();}
    void flush();
    [[nodiscard]] PageId page_count()const{return count_;}
};
}
