#include <navier/navier.hpp>
#include <chrono>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t number(const char* text){std::uint64_t value=0;const std::string_view input(text);const auto [end,error]=std::from_chars(input.data(),input.data()+input.size(),value);if(error!=std::errc{}||end!=input.data()+input.size())throw std::invalid_argument("Expected positive integer");return value;}
double elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
}
int main(int argc,char** argv){try{
    if(argc!=6){std::cerr<<"Usage: navier_storage_benchmark OUTPUT.data CHECKPOINTS NX NY FIELDS\nCreates a new NAVIER02 dataset, never overwrites existing files.\n";return 1;}
    const auto count=number(argv[2]),nx=number(argv[3]),ny=number(argv[4]),field_count=number(argv[5]);if(!count||!nx||!ny||!field_count||field_count>16||count>10000000)throw std::invalid_argument("Invalid benchmark dimensions/count");
    const auto max_cells=navier::format::checked_mul(navier::format::checked_add(nx,4),ny);if(max_cells>64*1024*1024)throw std::invalid_argument("Benchmark field generation exceeds 256 MiB budget");
    std::vector<float> values(navier::format::memory_size(max_cells));for(std::size_t i=0;i<values.size();++i)values[i]=static_cast<float>(std::sin(static_cast<double>(i)*.01));
    const std::filesystem::path path(argv[1]);double write_ms=0;std::uint64_t selected=0;
    {navier::storage::DatasetStore store(path,{.create=true,.cache_pages=64,.metadata="{\"solver\":\"synthetic-storage-v2\",\"purpose\":\"storage benchmark, not a physical simulation\"}"});const auto start=Clock::now();for(std::uint64_t i=0;i<count;++i){const auto x=nx+i%5,cells=x*ny;std::vector<navier::storage::FieldView> fields;for(std::uint64_t f=0;f<field_count;++f)fields.push_back({{"field_"+std::to_string(f),navier::storage::ScalarType::float32,{x,ny},"normalized"},std::span<const float>(values).first(navier::format::memory_size(cells))});selected=i*7+i%3;const std::array<navier::storage::Metric,1> metrics{{{"sequence",static_cast<double>(i),"index"}}};store.save(selected,static_cast<double>(i)*.01,fields,metrics);if(i%100==0)std::cerr<<"Written "<<i+1<<'/'<<count<<'\n';}write_ms=elapsed(start);}
    const auto open_start=Clock::now();navier::storage::DatasetStore store(path,{.cache_pages=64});const auto open_ms=elapsed(open_start);const auto index_bytes=std::filesystem::file_size(path.string()+".nvi"),bytes=std::filesystem::file_size(path);const auto opened=store.statistics();
    auto start=Clock::now();const auto checkpoint=store.get_checkpoint(selected);const auto exact_ms=elapsed(start);start=Clock::now();const auto field=store.load_field(selected,"field_0");const auto field_ms=elapsed(start);start=Clock::now();const auto range=store.range_steps(0,selected,1000);const auto range_ms=elapsed(start);
    start=Clock::now();for(int i=0;i<1000;++i)(void)store.get_checkpoint(selected);const auto warm_us=elapsed(start)*1000/1000;const auto stats=store.statistics();
    std::cout<<std::setprecision(12)<<"dataset_bytes,checkpoints,index_bytes,open_ms,open_records_scanned,open_dataset_bytes_read,write_ms,write_MiB_s,exact_descriptor_ms,field_read_ms,range_1000_ms,warm_descriptor_us,buffer_hits,buffer_misses,buffer_evictions,buffer_cached,field_reads\n"<<bytes<<','<<store.size()<<','<<index_bytes<<','<<open_ms<<','<<opened.records_scanned<<','<<opened.bytes_read<<','<<write_ms<<','<<(static_cast<double>(bytes)/1048576/(write_ms/1000))<<','<<exact_ms<<','<<field_ms<<','<<range_ms<<','<<warm_us<<','<<stats.buffer.hits<<','<<stats.buffer.misses<<','<<stats.buffer.evictions<<','<<stats.buffer.cached<<','<<stats.field_reads<<'\n';
    if(checkpoint.step!=selected||range.empty())throw std::runtime_error("Benchmark validation failed");(void)field;return 0;
}catch(const std::exception& error){std::cerr<<"Storage benchmark: "<<error.what()<<'\n';return 1;}}

