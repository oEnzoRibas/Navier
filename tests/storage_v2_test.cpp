#include <navier/navier.hpp>
#include <navier/format/binary.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <map>
#include <numeric>
#include <random>
#include <thread>
#include <atomic>
namespace {
using namespace navier::storage;
class V2Test:public testing::Test {
protected:
    std::filesystem::path directory,path;
    void SetUp()override{const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();directory=std::filesystem::temp_directory_path()/("navier-v2-"+std::to_string(stamp));
ASSERT_TRUE(std::filesystem::create_directory(directory));path=directory/"test.data";}
    void TearDown()override{std::error_code error;std::filesystem::remove_all(directory,error);}
    static std::vector<FieldView> fields(std::span<const float> a,std::span<const double> b){return {{{"arbitrary",ScalarType::float32,{2,3},"m"},a},{{"other",ScalarType::float64,{3,2},"s"},b}};}
    void corrupt(std::uint64_t at){std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);file.seekg(static_cast<std::streamoff>(at));char b=0;file.read(&b,1);b^=1;file.seekp(static_cast<std::streamoff>(at));file.write(&b,1);}
};
TEST_F(V2Test,MixedPrecisionFieldsAndMetadataPersistWithoutScan){
    const std::array<float,6> a{1,2,3,4,5,6};const std::array<double,6> b{.125,-0.,1e100,4,5,6};const std::array<Metric,1> m{{{"energy",12,"J"}}};
    {DatasetStore store(path,{.create=true,.cache_pages=2,.metadata="{\"solver\":\"test\"}"});store.save(7,.5,fields(a,b),m,CheckpointKind::restart);store.save(2,.1,fields(a,b));store.save(7,.75,fields(a,b),m);
EXPECT_EQ(store.size(),2U);store.validate(true);}
    DatasetStore store(path,{.cache_pages=2});
EXPECT_FALSE(store.statistics().index_rebuilt);
EXPECT_EQ(store.statistics().records_scanned,0U);
EXPECT_LT(store.statistics().bytes_read,2048U);
EXPECT_EQ(store.metadata(),"{\"solver\":\"test\"}");
    const auto before=store.statistics().bytes_read;auto value=store.load_field(7,"arbitrary");
EXPECT_EQ(std::get<std::vector<float>>(value.values),(std::vector<float>(a.begin(),a.end())));
EXPECT_LT(store.statistics().bytes_read-before,1024U);
EXPECT_EQ(store.statistics().field_reads,1U);
    const auto desc=store.get_checkpoint(7);
EXPECT_DOUBLE_EQ(desc.time,.75);
EXPECT_EQ(desc.metrics[0],m[0]);
EXPECT_EQ(desc.kind,CheckpointKind::analysis);
EXPECT_EQ(desc.fields[1].dimensions,(std::vector<std::uint64_t>{3,2}));
    EXPECT_EQ(store.range_steps(0,100).size(),2U);
EXPECT_EQ(store.range_steps(0,100,1)[0].step,2U);
EXPECT_TRUE(store.range_steps(100,0).empty());
EXPECT_THROW((void)store.load_field(7,"absent"),std::out_of_range);
EXPECT_EQ(store.load(7).fields.size(),2U);
}
TEST_F(V2Test,MemoryBudgetDoesNotPreventMetadataQueries){const std::array<float,6>a{};const std::array<double,6>b{};{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));}DatasetStore store(path,{.max_field_bytes=24});
EXPECT_EQ(store.get_checkpoint(1).fields.size(),2U);
EXPECT_NO_THROW((void)store.load_field(1,"arbitrary"));
EXPECT_THROW((void)store.load_field(1,"other"),std::length_error);
EXPECT_THROW((void)store.load(1),std::length_error);}
TEST_F(V2Test,ValidationRejectsBadInputsBeforeAppending){DatasetStore store(path,{.create=true});std::array<float,6>a{};std::array<double,6>b{};auto views=fields(a,b);const auto size=std::filesystem::file_size(path);views[0].descriptor.dimensions={3,3};
EXPECT_THROW(store.save(1,0,views),std::invalid_argument);views=fields(a,b);views[1].descriptor.name=views[0].descriptor.name;
EXPECT_THROW(store.save(1,0,views),std::invalid_argument);views=fields(a,b);views[0].descriptor.dimensions={std::numeric_limits<std::uint64_t>::max(),2};
EXPECT_THROW(store.save(1,0,views),std::runtime_error);
EXPECT_EQ(std::filesystem::file_size(path),size);}
TEST_F(V2Test,PayloadAndDirectoryCorruptionAreDetected){std::array<float,6>a{};std::array<double,6>b{};{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));}corrupt(std::filesystem::file_size(path)-16-48);{DatasetStore store(path);
EXPECT_NO_THROW(store.validate(false));
EXPECT_THROW((void)store.load_field(1,"other"),std::runtime_error);
EXPECT_THROW(store.validate(true),std::runtime_error);}corrupt(4096+64);DatasetStore store(path);
EXPECT_THROW((void)store.get_checkpoint(1),std::runtime_error);}
TEST_F(V2Test,TruncatedTailKeepsPrefixAndRequiresExplicitRepair){std::array<float,6>a{};std::array<double,6>b{};std::uint64_t prefix=0;{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));prefix=std::filesystem::file_size(path);store.save(2,1,fields(a,b));}std::filesystem::resize_file(path,std::filesystem::file_size(path)-5);{DatasetStore store(path);
EXPECT_TRUE(store.statistics().recovered_tail);
EXPECT_EQ(store.size(),1U);
EXPECT_NO_THROW((void)store.load_field(1,"arbitrary"));
EXPECT_THROW(store.save(3,2,fields(a,b)),std::runtime_error);store.repair_tail();
EXPECT_EQ(std::filesystem::file_size(path),prefix);store.save(3,2,fields(a,b));}DatasetStore store(path);
EXPECT_EQ(store.size(),2U);
EXPECT_FALSE(store.statistics().index_rebuilt);store.validate(true);}
TEST_F(V2Test,MissingOrDirtyIndexRebuildsFromHeadersOnly){std::array<float,6>a{};std::array<double,6>b{};{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));}std::filesystem::remove(path.string()+".nvi");{DatasetStore store(path);
EXPECT_TRUE(store.statistics().index_rebuilt);
EXPECT_EQ(store.statistics().records_scanned,1U);
EXPECT_EQ(store.statistics().field_reads,0U);store.validate(true);} {PageManager manager(path.string()+".nvi");auto header=manager.read_page(0);navier::format::put(header,40,1);navier::format::put(header,4092,navier::format::checksum(std::span(header).first(4092)),4);manager.write_page(0,header);manager.flush();}DatasetStore recovered(path);
EXPECT_TRUE(recovered.statistics().index_rebuilt);
EXPECT_EQ(recovered.size(),1U);}
TEST_F(V2Test,LegacyAndUnknownFormatsAreNotReinterpreted){std::array<double,1>a{1};{CheckpointStore legacy(path);legacy.save(0,0,a);}EXPECT_THROW(DatasetStore store(path),std::runtime_error);CheckpointStore legacy(path);
EXPECT_DOUBLE_EQ(legacy.load(0).values[0],1);
EXPECT_THROW(DatasetStore store(path,{.create=true}),std::runtime_error);}
TEST_F(V2Test,BoundedPinnedPoolAndDirtyEviction){PageManager manager(directory/"pages",true);for(int i=0;i<3;++i)(void)manager.allocate_page();BufferPool pool(manager,1);{auto first=pool.pin(0);first.page()[10]=std::byte{42};first.mark_dirty();
EXPECT_THROW((void)pool.pin(1),std::runtime_error);} {auto second=pool.pin(1);
EXPECT_EQ(second.page()[10],std::byte{0});}EXPECT_EQ(manager.read_page(0)[10],std::byte{42});
EXPECT_EQ(pool.statistics().dirty_writes,1U);
EXPECT_EQ(pool.statistics().evictions,1U);
EXPECT_LE(pool.statistics().cached,1U);}
TEST_F(V2Test,PersistentTreeMultiLevelReopenRangesAndReplacement){
    constexpr std::uint64_t count=50000;std::vector<std::uint64_t> keys(count);std::iota(keys.begin(),keys.end(),0);std::mt19937 random(42);std::shuffle(keys.begin(),keys.end(),random);
    {PageManager manager(directory/"index",true);BufferPool pool(manager,7);navier::index::PersistentBPlusTree tree(pool,true);for(const auto key:keys)tree.insert(key*7,{key+100,key+200});
EXPECT_TRUE(tree.validate());
EXPECT_EQ(tree.size(),count);tree.insert(7,{9,10});tree.commit(12);
EXPECT_GT(manager.page_count(),255U);
EXPECT_LE(pool.statistics().cached,7U);}
    {PageManager manager(directory/"index");BufferPool pool(manager,3);navier::index::PersistentBPlusTree tree(pool);
EXPECT_EQ(tree.generation(),12U);
EXPECT_EQ(tree.find(7),(std::optional<CheckpointLocation>{{9,10}}));
EXPECT_TRUE(tree.validate());std::uint64_t visited=0;tree.range(700,1400,[&](auto key,auto value){EXPECT_EQ(value.offset,key/7+100);++visited;return true;});
EXPECT_EQ(visited,101U);
EXPECT_FALSE(tree.find(8));}
}
TEST_F(V2Test,InterruptedPersistentUpdateIsRecognized){ {PageManager manager(directory/"index",true);BufferPool pool(manager,2);navier::index::PersistentBPlusTree tree(pool,true);tree.insert(1,{10,20});}PageManager manager(directory/"index");BufferPool pool(manager,2);
EXPECT_THROW(navier::index::PersistentBPlusTree tree(pool),std::runtime_error);}
TEST_F(V2Test,MalformedPagesFailWithoutUnsafeAllocations){ {PageManager manager(directory/"index",true);BufferPool pool(manager);navier::index::PersistentBPlusTree tree(pool,true);tree.commit(0);}PageManager manager(directory/"index");auto page=manager.read_page(1);page[4]=std::byte{255};navier::format::put(page,4092,navier::format::checksum(std::span(page).first(4092)),4);manager.write_page(1,page);manager.flush();BufferPool pool(manager);navier::index::PersistentBPlusTree tree(pool);
EXPECT_THROW((void)tree.find(1),std::runtime_error);
EXPECT_FALSE(tree.validate());}
TEST(BPlusTreeStress,DeterministicMixedOperationsAndClear){navier::index::BPlusTree<std::uint64_t,std::uint64_t,5> tree;std::map<std::uint64_t,std::uint64_t> oracle;std::mt19937_64 random(0x4e415649);for(std::uint64_t operation=0;operation<100000;++operation){const auto key=random()%30000,value=random();if(operation%5<3){EXPECT_EQ(tree.insert(key,value),!oracle.contains(key));oracle[key]=value;}else if(operation%5==3){const auto found=oracle.find(key);const auto actual=tree.find(key);if(found==oracle.end())EXPECT_EQ(actual,nullptr);else{ASSERT_NE(actual,nullptr);
EXPECT_EQ(*actual,found->second);}}else{const auto end=key+random()%100;std::vector<std::pair<std::uint64_t,std::uint64_t>> expected;for(auto it=oracle.lower_bound(key);it!=oracle.end()&&it->first<=end;++it)expected.push_back(*it);
EXPECT_EQ(tree.range(key,end),expected);}if(operation%1000==0){ASSERT_TRUE(tree.validate());
ASSERT_EQ(tree.size(),oracle.size());}}EXPECT_TRUE(tree.validate());tree.clear();
EXPECT_TRUE(tree.empty());
EXPECT_TRUE(tree.validate());tree.insert(0,1);tree.insert(std::numeric_limits<std::uint64_t>::max(),2);
EXPECT_TRUE(tree.validate());}
}


#include <navier/query/query_engine.hpp>
namespace {
TEST_F(V2Test,MetricIndexesUseCompositeKeysAndRejectStaleGenerations){
    const std::array<float,6>a{};const std::array<double,6>b{};DatasetStore store(path,{.create=true});
    for(const auto step:{1U,2U,3U}){const std::array<Metric,1> metric{{{"energy",step==3?20.:10.,"J"}}};store.save(step,static_cast<double>(step),fields(a,b),metric);}
    navier::query::QueryEngine query(store);
    EXPECT_THROW((void)query.query_metric("energy",0,30),std::runtime_error);
    const auto reads=store.statistics().field_reads;query.build_metric_index("energy");
    EXPECT_EQ(query.query_metric("energy",10,10).size(),2U);
    EXPECT_EQ(query.query_metric("energy",20,20)[0].step,3U);
    EXPECT_EQ(store.statistics().field_reads,reads);
    const std::array<Metric,1> metric{{{"energy",50,"J"}}};store.save(1,4,fields(a,b),metric);
    EXPECT_THROW((void)query.query_metric("energy",0,30),std::runtime_error);
    query.build_metric_index("energy");
    EXPECT_EQ(query.query_metric("energy",10,10).size(),1U);
    navier::query::QueryEngine small(store,1);
    EXPECT_THROW(small.build_metric_index("energy"),std::length_error);
}
TEST_F(V2Test,ConcurrentAccessToSameInstanceIsSerialized){const std::array<float,6>a{};const std::array<double,6>b{};DatasetStore store(path,{.create=true});store.save(0,0,fields(a,b));std::atomic<bool> failed=false;std::thread writer([&]{try{for(std::uint64_t i=1;i<50;++i)store.save(i,static_cast<double>(i),fields(a,b));}catch(...){failed=true;}});std::thread reader([&]{try{for(int i=0;i<100;++i)if(store.get_checkpoint(0).step!=0)failed=true;}catch(...){failed=true;}});writer.join();reader.join();EXPECT_FALSE(failed);store.validate(true);}
TEST_F(V2Test,SuperblockFallbackRecoversCompletedGeneration){const std::array<float,6>a{};const std::array<double,6>b{};{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));}corrupt(256+124);DatasetStore store(path);EXPECT_TRUE(store.statistics().index_rebuilt);EXPECT_EQ(store.size(),1U);EXPECT_EQ(store.get_checkpoint(1).generation,1U);}
TEST_F(V2Test,EveryTailTruncationBoundaryRecoversOnlyValidPrefix){const std::array<float,6>a{};const std::array<double,6>b{};std::uint64_t prefix=0;{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));prefix=std::filesystem::file_size(path);store.save(2,1,fields(a,b));}std::ifstream in(path,std::ios::binary);const std::string original((std::istreambuf_iterator<char>(in)),{});in.close();for(std::uint64_t cut=prefix;cut<original.size();++cut){{std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(original.data(),static_cast<std::streamsize>(cut));}DatasetStore store(path);EXPECT_EQ(store.size(),1U)<<cut;EXPECT_EQ(store.get_checkpoint(1).step,1U);EXPECT_EQ(store.statistics().field_reads,0U);}}
TEST_F(V2Test,RandomMalformedDirectoriesNeverAllocateUnboundedPayloads){const std::array<float,6>a{};const std::array<double,6>b{};{DatasetStore store(path,{.create=true});store.save(1,0,fields(a,b));}std::ifstream in(path,std::ios::binary);const std::string original((std::istreambuf_iterator<char>(in)),{});in.close();navier::format::Bytes valid(original.size());for(std::size_t i=0;i<original.size();++i)valid[i]=static_cast<std::byte>(original[i]);const auto dir_size=navier::format::memory_size(navier::format::get(valid,4096+48,4));std::mt19937 random(101);for(int trial=0;trial<200;++trial){auto bytes=valid;bytes[4096+64+random()%dir_size]^=std::byte{128};navier::format::put(bytes,4096+56,navier::format::checksum(std::span(bytes).subspan(4096+64,dir_size)),4);navier::format::put(bytes,4096+60,navier::format::checksum(std::span(bytes).subspan(4096,60)),4);{std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}try{DatasetStore store(path,{.max_field_bytes=1024});(void)store.load(1);}catch(const std::bad_alloc&){FAIL()<<"Corrupted directory attempted excessive allocation";}catch(const std::exception&){/* Explicit bounded parse failure is expected. */}}}
TEST_F(V2Test,InvalidChildAndBrokenLeafLinksAreRejected){
    {PageManager manager(directory/"index",true);BufferPool pool(manager);navier::index::PersistentBPlusTree tree(pool,true);for(std::uint64_t i=0;i<200;++i)tree.insert(i,{i,24});tree.commit(0);}
    PageManager manager(directory/"index");const auto original=manager.read_page(3);auto root=original;navier::format::put(root,32,manager.page_count()+1);navier::format::put(root,4092,navier::format::checksum(std::span(root).first(4092)),4);manager.write_page(3,root);manager.flush();
    {BufferPool pool(manager);navier::index::PersistentBPlusTree tree(pool);
    EXPECT_THROW((void)tree.find(0),std::runtime_error);
    EXPECT_FALSE(tree.validate());}
    manager.write_page(3,original);auto leaf=manager.read_page(1);navier::format::put(leaf,8,1);navier::format::put(leaf,4092,navier::format::checksum(std::span(leaf).first(4092)),4);manager.write_page(1,leaf);manager.flush();BufferPool pool(manager);navier::index::PersistentBPlusTree tree(pool);
    EXPECT_FALSE(tree.validate());
}
TEST(PortableFormat,IEEECRCReferenceAndIntegerBounds){const std::string text="123456789";EXPECT_EQ(navier::format::checksum(std::as_bytes(std::span(text))),0xcbf43926U);std::array<std::byte,8> bytes{};
EXPECT_THROW(navier::format::put(bytes,0,0,9),std::logic_error);
EXPECT_THROW((void)navier::format::get(bytes,1),std::runtime_error);
EXPECT_THROW((void)navier::format::checked_mul(std::numeric_limits<std::uint64_t>::max(),2),std::runtime_error);
}
}
