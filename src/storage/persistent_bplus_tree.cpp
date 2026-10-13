#include <navier/index/persistent_bplus_tree.hpp>
#include <navier/format/binary.hpp>
#include <algorithm>
#include <limits>
#include <unordered_set>
namespace navier::index {
namespace {constexpr std::size_t crc_at=storage::page_size-4,node_at=32;}
PersistentBPlusTree::PersistentBPlusTree(storage::BufferPool& pool,bool initialize,std::array<std::uint64_t,2> identity):pool_(pool),identity_(identity){
    if(initialize){if(pool_.page_count()!=0)throw std::runtime_error("Index initialization requires empty file");if(pool_.allocate()!=0)throw std::runtime_error("Invalid index superblock page");root_=pool_.allocate();write(root_,Node{});header();pool_.flush();return;}
    auto guard=pool_.pin(0);const auto& bytes=guard.page();
    if(!format::matches(bytes,0,"NVINDEX1")||format::get(bytes,8,4)!=1||format::get(bytes,12,4)!=storage::page_size||format::get(bytes,crc_at,4)!=format::checksum(std::span(bytes).first(crc_at)))throw std::runtime_error("Invalid index superblock/checksum");
    if(format::get(bytes,48)!=identity_[0]||format::get(bytes,56)!=identity_[1])throw std::runtime_error("Index belongs to another dataset");
    root_=format::get(bytes,16);size_=format::get(bytes,24);generation_=format::get(bytes,32);dirty_=format::get(bytes,40)!=0;
    if(dirty_)throw std::runtime_error("Interrupted index update requires rebuild");
    if(root_==0||root_>=pool_.page_count())throw std::runtime_error("Invalid persisted root PageId");
    // Root is validated lazily after header guard releases in normal lookups.
}
void PersistentBPlusTree::header(){auto guard=pool_.pin(0);auto& bytes=guard.page();bytes.fill(std::byte{0});format::magic(bytes,0,"NVINDEX1");format::put(bytes,8,1,4);format::put(bytes,12,storage::page_size,4);format::put(bytes,16,root_);format::put(bytes,24,size_);format::put(bytes,32,generation_);format::put(bytes,40,dirty_?1U:0U);format::put(bytes,48,identity_[0]);format::put(bytes,56,identity_[1]);format::put(bytes,crc_at,format::checksum(std::span(bytes).first(crc_at)),4);guard.mark_dirty();}
PersistentBPlusTree::Node PersistentBPlusTree::read(PageId id){
    if(!id||id>=pool_.page_count())throw std::runtime_error("Invalid child PageId");auto guard=pool_.pin(id);const auto& bytes=guard.page();
    if(!format::matches(bytes,0,"BPT1")||format::get(bytes,crc_at,4)!=format::checksum(std::span(bytes).first(crc_at)))throw std::runtime_error("Invalid B+ Tree page/checksum");
    const auto type=format::get(bytes,4,1),count=format::memory_size(format::get(bytes,6,2));if(type>1||count>(type==1?leaf_capacity:internal_capacity))throw std::runtime_error("Invalid page type/count");
    Node node;node.leaf=type==1;node.next=format::get(bytes,8);if(node.next>=pool_.page_count()||node.next==id||(!node.leaf&&node.next))throw std::runtime_error("Invalid leaf link");
    std::size_t at=node_at;
    if(!node.leaf){const auto child=format::get(bytes,at);if(!child||child>=pool_.page_count()||child==id)throw std::runtime_error("Invalid child PageId");node.children.push_back(child);at+=8;}
    for(std::size_t i=0;i<count;++i){node.keys.push_back(format::get(bytes,at));at+=8;if(i&&node.keys[i]<=node.keys[i-1])throw std::runtime_error("Unsorted B+ Tree page");if(node.leaf){node.values.push_back({format::get(bytes,at),format::get(bytes,at+8)});at+=16;}else{const auto child=format::get(bytes,at);if(!child||child>=pool_.page_count()||child==id)throw std::runtime_error("Invalid child PageId");node.children.push_back(child);at+=8;}}
    return node;
}
void PersistentBPlusTree::write(PageId id,const Node& node){
    if(node.keys.size()>(node.leaf?leaf_capacity:internal_capacity)||(!node.leaf&&node.children.size()!=node.keys.size()+1)||(node.leaf&&node.values.size()!=node.keys.size()))throw std::logic_error("Invalid node serialization");
    auto guard=pool_.pin(id);auto& bytes=guard.page();bytes.fill(std::byte{0});format::magic(bytes,0,"BPT1");format::put(bytes,4,node.leaf?1U:0U,1);format::put(bytes,6,node.keys.size(),2);format::put(bytes,8,node.next);std::size_t at=node_at;
    if(!node.leaf){format::put(bytes,at,node.children[0]);at+=8;}
    for(std::size_t i=0;i<node.keys.size();++i){format::put(bytes,at,node.keys[i]);at+=8;if(node.leaf){format::put(bytes,at,node.values[i].offset);format::put(bytes,at+8,node.values[i].size);at+=16;}else{format::put(bytes,at,node.children[i+1]);at+=8;}}
    format::put(bytes,crc_at,format::checksum(std::span(bytes).first(crc_at)),4);guard.mark_dirty();
}
void PersistentBPlusTree::begin_update(){if(dirty_)return;dirty_=true;header();pool_.flush();}
std::optional<PersistentBPlusTree::Split> PersistentBPlusTree::insert_into(PageId id,std::uint64_t key,storage::CheckpointLocation value,bool& inserted,unsigned depth){
    if(depth>63)throw std::runtime_error("Cyclic/deep insertion path");
    auto node=read(id);
    if(node.leaf){const auto it=std::lower_bound(node.keys.begin(),node.keys.end(),key);const auto pos=static_cast<std::size_t>(it-node.keys.begin());if(it!=node.keys.end()&&*it==key){node.values[pos]=value;write(id,node);return std::nullopt;}
        node.keys.insert(it,key);node.values.insert(node.values.begin()+static_cast<std::ptrdiff_t>(pos),value);inserted=true;
        if(node.keys.size()<=leaf_capacity){write(id,node);return std::nullopt;}
        const auto mid=node.keys.size()/2;Node right;right.next=node.next;right.keys.assign(node.keys.begin()+static_cast<std::ptrdiff_t>(mid),node.keys.end());right.values.assign(node.values.begin()+static_cast<std::ptrdiff_t>(mid),node.values.end());node.keys.resize(mid);node.values.resize(mid);const auto right_id=pool_.allocate();node.next=right_id;write(id,node);write(right_id,right);return Split{right.keys.front(),right_id};
    }
    const auto pos=static_cast<std::size_t>(std::upper_bound(node.keys.begin(),node.keys.end(),key)-node.keys.begin());const auto split=insert_into(node.children[pos],key,value,inserted,depth+1);if(!split)return std::nullopt;
    node.keys.insert(node.keys.begin()+static_cast<std::ptrdiff_t>(pos),split->key);node.children.insert(node.children.begin()+static_cast<std::ptrdiff_t>(pos+1),split->right);
    if(node.keys.size()<=internal_capacity){write(id,node);return std::nullopt;}
    const auto mid=node.keys.size()/2;const auto promoted=node.keys[mid];Node right;right.leaf=false;right.keys.assign(node.keys.begin()+static_cast<std::ptrdiff_t>(mid+1),node.keys.end());right.children.assign(node.children.begin()+static_cast<std::ptrdiff_t>(mid+1),node.children.end());node.keys.resize(mid);node.children.resize(mid+1);const auto right_id=pool_.allocate();write(id,node);write(right_id,right);return Split{promoted,right_id};
}
bool PersistentBPlusTree::insert(std::uint64_t key,storage::CheckpointLocation value){begin_update();bool inserted=false;const auto split=insert_into(root_,key,value,inserted);if(split){Node root;root.leaf=false;root.keys={split->key};root.children={root_,split->right};root_=pool_.allocate();write(root_,root);}if(inserted)++size_;return inserted;}
void PersistentBPlusTree::commit(std::uint64_t generation){pool_.flush();generation_=generation;dirty_=false;header();pool_.flush();}
std::optional<storage::CheckpointLocation> PersistentBPlusTree::find(std::uint64_t key){
    auto id=root_;for(std::uint64_t depth=0;depth<64;++depth){const auto node=read(id);if(node.leaf){const auto it=std::lower_bound(node.keys.begin(),node.keys.end(),key);if(it==node.keys.end()||*it!=key)return std::nullopt;return node.values[static_cast<std::size_t>(it-node.keys.begin())];}id=node.children[static_cast<std::size_t>(std::upper_bound(node.keys.begin(),node.keys.end(),key)-node.keys.begin())];}throw std::runtime_error("Cyclic/deep B+ Tree path");
}
void PersistentBPlusTree::range(std::uint64_t begin,std::uint64_t end,const std::function<bool(std::uint64_t,storage::CheckpointLocation)>& visit){
    if(begin>end)return;auto id=root_;Node node;bool leaf=false;for(int depth=0;depth<64;++depth){node=read(id);if(node.leaf){leaf=true;break;}id=node.children[static_cast<std::size_t>(std::upper_bound(node.keys.begin(),node.keys.end(),begin)-node.keys.begin())];}if(!leaf)throw std::runtime_error("Cyclic/deep B+ Tree path");
    std::optional<std::uint64_t> previous;for(PageId pages=0;;++pages){if(pages>=pool_.page_count()||!node.leaf)throw std::runtime_error("Broken leaf chain");if(previous&&(!node.keys.empty()&&node.keys.front()<=*previous))throw std::runtime_error("Unordered leaf chain");for(std::size_t i=0;i<node.keys.size();++i){const auto key=node.keys[i];if(key<begin)continue;if(key>end||!visit(key,node.values[i]))return;}if(!node.next)return;if(!node.keys.empty())previous=node.keys.back();node=read(node.next);}
}
bool PersistentBPlusTree::validate(){
    try{
        struct Extent {std::uint64_t min=0,max=0,count=0;int depth=0;};std::unordered_set<PageId> seen;std::vector<std::pair<PageId,PageId>> leaves;
        const std::function<Extent(PageId,int)> walk=[&](PageId id,int depth)->Extent{
            if(depth>63||!seen.insert(id).second)throw std::runtime_error("Duplicate/cyclic page reference");const auto n=read(id);const bool root=id==root_;
            if(!root&&n.keys.size()<(n.leaf?(leaf_capacity+1)/2:internal_capacity/2))throw std::runtime_error("Underfull node");
            if(n.leaf){leaves.emplace_back(id,n.next);return {n.keys.empty()?0:n.keys.front(),n.keys.empty()?0:n.keys.back(),n.keys.size(),depth};}
            if(n.keys.empty())throw std::runtime_error("Empty internal root");auto result=walk(n.children[0],depth+1);for(std::size_t i=1;i<n.children.size();++i){const auto child=walk(n.children[i],depth+1);if(child.depth!=result.depth||child.min!=n.keys[i-1]||result.max>=child.min)throw std::runtime_error("Invalid separator/depth");result.max=child.max;result.count+=child.count;}return result;
        };
        const auto result=walk(root_,0);if(result.count!=size_)return false;for(std::size_t i=0;i<leaves.size();++i)if(leaves[i].second!=(i+1<leaves.size()?leaves[i+1].first:0))return false;return true;
    }catch(const std::runtime_error&){return false;}
}
}
