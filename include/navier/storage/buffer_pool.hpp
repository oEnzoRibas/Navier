#pragma once
#include <navier/storage/page_manager.hpp>
#include <list>
#include <stdexcept>
#include <unordered_map>
namespace navier::storage {
// Single-threaded pool. Guards must not outlive their owner.
class BufferPool {
    struct Entry {Page page;bool dirty=false;std::size_t pins=0;std::list<PageId>::iterator recent;};
    PageManager& manager_;std::size_t capacity_;std::unordered_map<PageId,Entry> entries_;std::list<PageId> recent_;BufferStatistics statistics_;
    void evict();void unpin(PageId id)noexcept;
public:
    class Guard {
        BufferPool* pool_=nullptr;PageId id_=0;
        friend class BufferPool;
        Guard(BufferPool* pool,PageId id):pool_(pool),id_(id){}
    public:
        Guard(const Guard&)=delete;Guard& operator=(const Guard&)=delete;
        Guard(Guard&& other)noexcept:pool_(other.pool_),id_(other.id_){other.pool_=nullptr;}
        Guard& operator=(Guard&&)=delete;
        ~Guard(){if(pool_)pool_->unpin(id_);}
        Page& page(){return pool_->entries_.at(id_).page;}
        void mark_dirty(){pool_->entries_.at(id_).dirty=true;}
    };
    explicit BufferPool(PageManager& manager,std::size_t capacity=64):manager_(manager),capacity_(capacity){if(!capacity)throw std::invalid_argument("Buffer pool requires at least one page");}
    ~BufferPool(){try{flush();}catch(...){/* Explicit commit/flush reports errors; destructors cannot. */}}
    BufferPool(const BufferPool&)=delete;BufferPool& operator=(const BufferPool&)=delete;
    [[nodiscard]] Guard pin(PageId id);
    [[nodiscard]] PageId allocate(){return manager_.allocate_page();}
    [[nodiscard]] PageId page_count()const{return manager_.page_count();}
    void flush();
    [[nodiscard]] BufferStatistics statistics()const;
};
}
