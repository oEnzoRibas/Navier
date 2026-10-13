#include <navier/navier.hpp>
#include <navier/format/file.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <numeric>
#include <thread>

namespace {
using namespace navier::storage;
class RegionTest : public testing::Test {
protected:
    std::filesystem::path directory,path;
    void SetUp() override {
        directory=std::filesystem::temp_directory_path()/("navier-region-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));path=directory/"test.data";
    }
    void TearDown() override {std::error_code error;std::filesystem::remove_all(directory,error);}
    static void flip(const std::filesystem::path& file,std::uint64_t offset) {
        std::fstream stream(file,std::ios::binary|std::ios::in|std::ios::out);stream.seekg(static_cast<std::streamoff>(offset));
        char value=0;stream.read(&value,1);value^=1;stream.seekp(static_cast<std::streamoff>(offset));stream.write(&value,1);
    }
};
TEST_F(RegionTest,StridedRegionsMatchWholeFieldAndWarmReadsSkipUnneededBlocks) {
    constexpr std::uint64_t nx=128,ny=128,nz=16;
    std::vector<float> values(nx*ny*nz);std::iota(values.begin(),values.end(),0.F);
    const FieldView view{{"temperature",ScalarType::float32,{nx,ny,nz},"K"},std::span<const float>(values)};
    {DatasetStore store(path,{.create=true});store.save(4,.4,std::span(&view,1));}
    const FieldRegion region{{3,5,7},{4,6,1},{2,3,1}};
    {DatasetStore store(path,{.max_field_bytes=128});
        const auto result=store.load_region(4,"temperature",region);
        const auto& data=std::get<std::vector<float>>(result.values);
        ASSERT_EQ(data.size(),24U);EXPECT_EQ(result.descriptor.dimensions,region.count);
        for(std::size_t y=0;y<6;++y)for(std::size_t x=0;x<4;++x)EXPECT_EQ(data[y*4+x],values[(7*ny+5+y*3)*nx+3+x*2]);
        EXPECT_THROW((void)store.load_field(4,"temperature"),std::length_error);
    }
    DatasetStore reopened(path,{.max_field_bytes=128});
    const auto before=reopened.statistics().bytes_read;
    const auto result=reopened.load_region(4,"temperature",region);
    EXPECT_EQ(std::get<std::vector<float>>(result.values).size(),24U);
    const auto bytes=reopened.statistics().bytes_read-before;
    EXPECT_LT(bytes,70000U); // 1 verified block + bounded record directory, not 1 MiB.
    EXPECT_FALSE(reopened.statistics().index_rebuilt);
}
TEST_F(RegionTest,AxesAndFloat64BitsRemainCorrect) {
    std::vector<double> values(5*6*7);std::iota(values.begin(),values.end(),0.);
    values[0]=-0.;values[1]=std::bit_cast<double>(std::uint64_t{0x7ff8123456789abcULL});
    const FieldView view{{"arbitrary",ScalarType::float64,{5,6,7},"m"},std::span<const double>(values)};
    DatasetStore store(path,{.create=true});store.save(1,1,std::span(&view,1));
    for(std::size_t axis=0;axis<3;++axis) {
        FieldRegion region{{0,0,0},{5,6,7},{1,1,1}};region.origin[axis]=2;region.count[axis]=1;
        const auto result=store.load_region(1,"arbitrary",region);const auto& data=std::get<std::vector<double>>(result.values);
        std::size_t at=0;
        for(std::uint64_t z=0;z<region.count[2];++z)for(std::uint64_t y=0;y<region.count[1];++y)for(std::uint64_t x=0;x<region.count[0];++x)
            EXPECT_EQ(data[at++],values[((z+region.origin[2])*6+y+region.origin[1])*5+x+region.origin[0]]);
    }
    const auto first=store.load_region(1,"arbitrary",{{0,0,0},{2,1,1},{1,1,1}});
    EXPECT_TRUE(std::signbit(std::get<std::vector<double>>(first.values)[0]));
    EXPECT_TRUE(std::isnan(std::get<std::vector<double>>(first.values)[1]));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(std::get<std::vector<double>>(first.values)[1]),0x7ff8123456789abcULL);
}
TEST_F(RegionTest,BoundsOverflowAndBudgetsFailBeforeReadingPayload) {
    const std::array<float,8> values{};const FieldView view{{"f",ScalarType::float32,{2,2,2},""},std::span<const float>(values)};
    DatasetStore store(path,{.create=true,.max_field_bytes=4});store.save(0,0,std::span(&view,1));
    EXPECT_THROW((void)store.load_region(0,"f",{{0},{1},{1}}),std::invalid_argument);
    EXPECT_THROW((void)store.load_region(0,"f",{{0,0,0},{0,1,1},{1,1,1}}),std::invalid_argument);
    EXPECT_THROW((void)store.load_region(0,"f",{{0,0,0},{1,1,1},{0,1,1}}),std::invalid_argument);
    EXPECT_THROW((void)store.load_region(0,"f",{{2,0,0},{1,1,1},{1,1,1}}),std::out_of_range);
    EXPECT_THROW((void)store.load_region(0,"f",{{0,0,0},{2,2,2},{1,1,1}}),std::length_error);
    EXPECT_THROW((void)store.load_region(0,"f",{{0,0,0},{2,1,1},{std::numeric_limits<std::uint64_t>::max(),1,1}}),std::exception);
    EXPECT_FALSE(std::filesystem::exists(path.string()+".nvb"));
}
TEST_F(RegionTest,CorruptionInRequestedBlockIsDetectedAfterReopen) {
    std::vector<float> values(32768,1.F);const FieldView view{{"f",ScalarType::float32,{32768},""},std::span<const float>(values)};
    {DatasetStore store(path,{.create=true});store.save(0,0,std::span(&view,1));(void)store.load_region(0,"f",{{0},{1},{1}});}
    const auto cache=std::filesystem::directory_iterator(path.string()+".nvb")->path();
    navier::format::File index(cache);const auto head=index.read(0,128);const auto field_offset=navier::format::get(head,40);
    flip(path,field_offset);
    DatasetStore store(path);
    EXPECT_THROW((void)store.load_region(0,"f",{{0},{1},{1}}),std::runtime_error);
    // A different block is still readable. Full validation is the whole-history audit.
    EXPECT_EQ(std::get<std::vector<float>>(store.load_region(0,"f",{{20000},{1},{1}}).values)[0],1.F);
    EXPECT_THROW(store.validate(true),std::runtime_error);
}
TEST_F(RegionTest,DamagedDerivedCacheRebuildsAndDuplicateStepsUseNewRecord) {
    std::vector<float> values(32,1.F);const FieldView view{{"f",ScalarType::float32,{32},""},std::span<const float>(values)};
    DatasetStore store(path,{.create=true});store.save(5,1,std::span(&view,1));(void)store.load_region(5,"f",{{0},{1},{1}});
    const auto cache=std::filesystem::directory_iterator(path.string()+".nvb")->path();flip(cache,128);
    EXPECT_EQ(std::get<std::vector<float>>(store.load_region(5,"f",{{0},{1},{1}}).values)[0],1.F);
    std::fill(values.begin(),values.end(),2.F);store.save(5,2,std::span(&view,1));
    EXPECT_EQ(std::get<std::vector<float>>(store.load_region(5,"f",{{0},{1},{1}}).values)[0],2.F);
}
TEST_F(RegionTest,ColdBuildValidatesUnrequestedDataAndHigherRankCoordinatesWork) {
    std::vector<double> values(2*3*4*5);std::iota(values.begin(),values.end(),0.);
    const FieldView view{{"f",ScalarType::float64,{2,3,4,5},""},std::span<const double>(values)};
    {DatasetStore store(path,{.create=true});store.save(0,0,std::span(&view,1));
        const auto region=store.load_region(0,"f",{{1,1,1,2},{1,2,2,2},{1,1,2,1}});
        const auto& data=std::get<std::vector<double>>(region.values);std::size_t at=0;
        for(std::uint64_t w=2;w<=3;++w)for(std::uint64_t z=1;z<=3;z+=2)for(std::uint64_t y=1;y<=2;++y)
            EXPECT_EQ(data[at++],values[((w*4+z)*3+y)*2+1]);
    }
    // Remove only the derived cache, then corrupt an unrequested byte.
    std::filesystem::remove_all(path.string()+".nvb");
    std::uint64_t payload=0;
    {navier::format::File file(path);const auto head=file.read(4096,64);payload=4096+64+navier::format::get(head,48,4);}
    flip(path,payload+values.size()*8-1);
    DatasetStore store(path);
    EXPECT_THROW((void)store.load_region(0,"f",{{0,0,0,0},{1,1,1,1},{1,1,1,1}}),std::runtime_error);
}
TEST_F(RegionTest,ConcurrentReadInstancesCanPublishTheSameDerivedDirectory) {
    std::vector<float> values(262144,3.F);const FieldView view{{"f",ScalarType::float32,{262144},""},std::span<const float>(values)};
    {DatasetStore store(path,{.create=true});store.save(0,0,std::span(&view,1));}
    DatasetStore first(path),second(path);std::exception_ptr failure_a,failure_b;
    std::thread a([&]{try{(void)first.load_region(0,"f",{{0},{1},{1}});}catch(...){failure_a=std::current_exception();}});
    std::thread b([&]{try{(void)second.load_region(0,"f",{{0},{1},{1}});}catch(...){failure_b=std::current_exception();}});
    a.join();b.join();EXPECT_FALSE(failure_a);EXPECT_FALSE(failure_b);
    EXPECT_EQ(std::get<std::vector<float>>(first.load_region(0,"f",{{0},{1},{1}}).values)[0],3.F);
}
}
