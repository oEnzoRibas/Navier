#include <navier/storage/buffer_pool.hpp>
#include <stdexcept>
namespace navier::storage {
void BufferPool::evict(){
    for(auto it=recent_.rbegin();it!=recent_.rend();++it){auto found=entries_.find(*it);if(found->second.pins)continue;if(found->second.dirty){manager_.write_page(found->first,found->second.page);++statistics_.dirty_writes;}recent_.erase(found->second.recent);entries_.erase(found);++statistics_.evictions;return;}
    throw std::runtime_error("All buffer pages are pinned");
}
BufferPool::Guard BufferPool::pin(PageId id){
    auto found=entries_.find(id);
    if(found==entries_.end()){++statistics_.misses;if(entries_.size()==capacity_)evict();auto page=manager_.read_page(id);recent_.push_front(id);found=entries_.emplace(id,Entry{page,false,0,recent_.begin()}).first;}
    else{++statistics_.hits;recent_.splice(recent_.begin(),recent_,found->second.recent);found->second.recent=recent_.begin();}
    ++found->second.pins;return Guard(this,id);
}
void BufferPool::unpin(PageId id)noexcept{const auto found=entries_.find(id);if(found!=entries_.end()&&found->second.pins)--found->second.pins;}
void BufferPool::flush(){for(auto& [id,entry]:entries_){if(!entry.dirty)continue;if(entry.pins)throw std::runtime_error("Cannot flush pinned dirty page");manager_.write_page(id,entry.page);entry.dirty=false;++statistics_.dirty_writes;}manager_.flush();}
BufferStatistics BufferPool::statistics()const{auto result=statistics_;result.cached=entries_.size();for(const auto& [id,entry]:entries_){(void)id;if(entry.pins)++result.pinned;}return result;}
}
