#pragma once
#include <navier/storage/buffer_pool.hpp>
#include <navier/storage/checkpoint.hpp>
#include <functional>
#include <array>
#include <optional>
#include <utility>
#include <vector>
namespace navier::index {
// Fixed uint64 temporal keys and record references; pages are portable, never raw Nodes.
class PersistentBPlusTree {
    using PageId=storage::PageId;
    struct Node {bool leaf=true;std::vector<std::uint64_t> keys;std::vector<storage::CheckpointLocation> values;std::vector<PageId> children;PageId next=0;};
    struct Split {std::uint64_t key;PageId right;};
    storage::BufferPool& pool_;PageId root_=0;std::uint64_t size_=0,generation_=0;bool dirty_=false;std::array<std::uint64_t,2> identity_{};
    [[nodiscard]] Node read(PageId id);
    void write(PageId id,const Node& node);
    void header();
    [[nodiscard]] std::optional<Split> insert_into(PageId id,std::uint64_t key,storage::CheckpointLocation value,bool& inserted,unsigned depth=0);
public:
    static constexpr std::size_t leaf_capacity=169,internal_capacity=253;
    explicit PersistentBPlusTree(storage::BufferPool& pool,bool initialize=false,std::array<std::uint64_t,2> identity={});
    void begin_update();
    bool insert(std::uint64_t key,storage::CheckpointLocation value);
    void commit(std::uint64_t generation);
    [[nodiscard]] std::optional<storage::CheckpointLocation> find(std::uint64_t key);
    void range(std::uint64_t begin,std::uint64_t end,const std::function<bool(std::uint64_t,storage::CheckpointLocation)>& visit);
    [[nodiscard]] bool validate();
    [[nodiscard]] PageId root_page()const{return root_;}
    [[nodiscard]] std::uint64_t size()const{return size_;}
    [[nodiscard]] std::uint64_t generation()const{return generation_;}
};
}
