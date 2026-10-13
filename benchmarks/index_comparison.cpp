#include <navier/index/bplus_tree.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <unordered_map>
#include <vector>
namespace {
using Key=std::uint64_t;using Pair=std::pair<Key,Key>;using Clock=std::chrono::steady_clock;
struct Adapter {std::function<void(const std::vector<Pair>&)> build;std::function<Key(Key)> find;std::function<void(Pair)> insert;std::function<std::vector<Pair>(Key,Key)> range;};
double ns(Clock::time_point start,std::size_t count){return std::chrono::duration<double,std::nano>(Clock::now()-start).count()/static_cast<double>(count);}
Key checksum(const std::vector<Pair>& entries){Key value=0;for(const auto& [key,item]:entries)value+=key^item;return value;}
void run(const char* name,std::size_t n,int trial,Adapter adapter,const std::vector<Pair>& entries,const std::vector<Key>& lookups,const std::vector<Pair>& extra){
    auto start=Clock::now();adapter.build(entries);const auto build=ns(start,n);Key sum=0;start=Clock::now();for(const auto key:lookups)sum+=adapter.find(key);const auto lookup=ns(start,lookups.size());
    Key expected=0;for(const auto key:lookups)if(key%2==0&&key/2<n)expected+=key+1;if(sum!=expected)throw std::runtime_error("Benchmark exact lookup mismatch");
    auto emit=[&](const char* operation,double elapsed,Key check){std::cout<<name<<','<<n<<','<<trial<<','<<operation<<','<<elapsed<<','<<check<<'\n';};emit("build_ns_per_entry",build,n);emit("exact_ns_per_query",lookup,sum);
    start=Clock::now();sum=0;for(std::size_t i=0;i<16;++i){const auto begin=lookups[i]%static_cast<Key>(n*2);sum+=checksum(adapter.range(begin,begin+127));}emit("range_ns_per_query",ns(start,16),sum);
    start=Clock::now();const auto ordered=adapter.range(0,std::numeric_limits<Key>::max());const auto traversal=ns(start,n);if(ordered.size()!=n||!std::is_sorted(ordered.begin(),ordered.end()))throw std::runtime_error("Ordered traversal mismatch");emit("ordered_traversal_ns_per_entry",traversal,checksum(ordered));
    start=Clock::now();for(const auto pair:extra)adapter.insert(pair);emit("dynamic_insert_ns_per_entry",ns(start,extra.size()),extra.size());
}
}
int main(int argc,char** argv){try{std::size_t maximum=1000000;int trials=3;if(argc>1){const std::string_view input(argv[1]);const auto [end,error]=std::from_chars(input.data(),input.data()+input.size(),maximum);if(error!=std::errc{}||end!=input.data()+input.size()||maximum>10000000)throw std::invalid_argument("Invalid maximum size");}std::cout<<std::setprecision(12)<<"structure,entries,trial,operation,ns,checksum\n";
    for(const std::size_t n:{1000U,10000U,100000U,1000000U}){if(n>maximum)continue;std::mt19937_64 random(0x4e415649);std::vector<Pair> entries(n);for(std::size_t i=0;i<n;++i)entries[i]={i*2,i*2+1};std::shuffle(entries.begin(),entries.end(),random);std::vector<Key> lookups(256);for(auto& key:lookups)key=random()%(n*2+100);std::vector<Pair> extra;for(std::size_t i=0;i<128;++i){const Key key=2*(i*n/128)+1;extra.emplace_back(key,key+1);}
    for(int trial=0;trial<trials;++trial){
        std::vector<Pair> linear;run("linear",n,trial,{[&](const auto& e){linear=e;},[&](Key k){for(const auto& p:linear)if(p.first==k)return p.second;return Key{0};},[&](Pair p){linear.push_back(p);},[&](Key a,Key b){std::vector<Pair> result;for(auto p:linear)if(p.first>=a&&p.first<=b)result.push_back(p);std::sort(result.begin(),result.end());return result;}},entries,lookups,extra);
        std::vector<Pair> sorted;run("sorted_vector_bulk_sort",n,trial,{[&](const auto& e){sorted=e;std::sort(sorted.begin(),sorted.end());},[&](Key k){const auto it=std::lower_bound(sorted.begin(),sorted.end(),Pair{k,0});return it!=sorted.end()&&it->first==k?it->second:0;},[&](Pair p){sorted.insert(std::lower_bound(sorted.begin(),sorted.end(),p),p);},[&](Key a,Key b){const auto first=std::lower_bound(sorted.begin(),sorted.end(),Pair{a,0});const auto last=std::upper_bound(sorted.begin(),sorted.end(),Pair{b,std::numeric_limits<Key>::max()});return std::vector<Pair>(first,last);}},entries,lookups,extra);
        std::map<Key,Key> map;run("std_map",n,trial,{[&](const auto& e){for(const auto& p:e)map.insert(p);},[&](Key k){const auto it=map.find(k);return it==map.end()?0:it->second;},[&](Pair p){map.insert(p);},[&](Key a,Key b){std::vector<Pair> result;for(auto it=map.lower_bound(a);it!=map.end()&&it->first<=b;++it)result.push_back(*it);return result;}},entries,lookups,extra);
        std::unordered_map<Key,Key> hash;run("unordered_map_scan_sort_ranges",n,trial,{[&](const auto& e){hash.reserve(e.size());for(const auto& p:e)hash.insert(p);},[&](Key k){const auto it=hash.find(k);return it==hash.end()?0:it->second;},[&](Pair p){hash.insert(p);},[&](Key a,Key b){std::vector<Pair> result;for(auto p:hash)if(p.first>=a&&p.first<=b)result.emplace_back(p);std::sort(result.begin(),result.end());return result;}},entries,lookups,extra);
        navier::index::BPlusTree<Key,Key> tree;run("navier_bplus_tree",n,trial,{[&](const auto& e){for(const auto& p:e)tree.insert(p.first,p.second);},[&](Key k){const auto p=tree.find(k);return p?*p:0;},[&](Pair p){tree.insert(p.first,p.second);},[&](Key a,Key b){return tree.range(a,b);}},entries,lookups,extra);
    }}return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
