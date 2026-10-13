#pragma once
#include <navier/storage/dataset_store.hpp>
#include <navier/index/bplus_tree.hpp>
#include <mutex>
#include <unordered_map>
namespace navier::query {
struct MetricKey {double value;std::uint64_t step;};
struct MetricCompare {bool operator()(const MetricKey& a,const MetricKey& b)const{return a.value<b.value||(a.value==b.value&&a.step<b.step);}};
// Explicitly built, bounded RAM secondary index. Temporal queries use persistent storage index.
class QueryEngine {
    using MetricTree=index::BPlusTree<MetricKey,std::uint64_t,32,MetricCompare>;
    struct Index {std::uint64_t generation;std::unique_ptr<MetricTree> tree;};
    storage::DatasetStore& store_;std::size_t max_entries_;std::unordered_map<std::string,Index> indexes_;std::mutex mutex_;
public:
    explicit QueryEngine(storage::DatasetStore& store,std::size_t max_entries=100000):store_(store),max_entries_(max_entries){}
    void build_metric_index(const std::string& name){
        std::lock_guard lock(mutex_);if(indexes_.size()>=8&&!indexes_.contains(name))throw std::length_error("Close/clear metric indexes before building another (limit 8)");const auto generation=store_.generation();auto tree=std::make_unique<MetricTree>();std::uint64_t begin=0;
        for(;;){const auto page=store_.range_steps(begin,std::numeric_limits<std::uint64_t>::max(),1000);if(page.empty())break;for(const auto& cp:page)for(const auto& metric:cp.metrics)if(metric.name==name){if(tree->size()>=max_entries_)throw std::length_error("Metric index entry budget exceeded");tree->insert({metric.value,cp.step},cp.step);}if(page.back().step==std::numeric_limits<std::uint64_t>::max())break;begin=page.back().step+1;}
        if(store_.generation()!=generation)throw std::runtime_error("Dataset changed during metric index build");indexes_.insert_or_assign(name,Index{generation,std::move(tree)});
    }
    [[nodiscard]] std::vector<storage::CheckpointDescriptor> query_metric(const std::string& name,double minimum,double maximum,std::size_t limit=1000){
        std::lock_guard lock(mutex_);if(!std::isfinite(minimum)||!std::isfinite(maximum)||limit>100000)throw std::invalid_argument("Invalid metric query bounds/limit");const auto found=indexes_.find(name);if(found==indexes_.end())throw std::runtime_error("Explicitly build_metric_index before querying (metadata scan required once)");const auto generation=store_.generation();if(found->second.generation!=generation)throw std::runtime_error("Metric index stale: rebuild after writes");std::vector<storage::CheckpointDescriptor> result;if(minimum>maximum||!limit)return result;
        for(const auto& [key,step]:found->second.tree->range({minimum,0},{maximum,std::numeric_limits<std::uint64_t>::max()})){(void)key;if(result.size()==limit)break;result.push_back(store_.get_checkpoint(step));}if(store_.generation()!=generation)throw std::runtime_error("Dataset changed during metric query");return result;
    }
    void clear(){std::lock_guard lock(mutex_);indexes_.clear();}
};
}
