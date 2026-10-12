#include <navier/navier.hpp>
#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <chrono>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace {
class StoreTest : public testing::Test {
protected:
    std::filesystem::path directory;
    std::filesystem::path path;
    void SetUp() override {
        const auto base = std::filesystem::temp_directory_path();
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            directory = base / ("navier-test-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(directory)) { path = directory / "sim.data"; return; }
        }
        FAIL() << "Cannot create temporary directory";
    }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
};
using navier::storage::CheckpointStore;
TEST_F(StoreTest, EmptyAndMissing) {
    CheckpointStore store(path);
    EXPECT_EQ(store.size(), 0U);
    EXPECT_TRUE(store.range(0, 100).empty());
    EXPECT_FALSE(store.location(100).has_value());
    EXPECT_THROW((void)store.load(100), std::out_of_range);
    EXPECT_EQ(std::filesystem::file_size(path), 8U);
}
TEST_F(StoreTest, RoundTripAndLocations) {
    CheckpointStore store(path);
    const std::vector<double> values{1.5, -2.25, 0.0, 1e100};
    store.save(100, 0.5, values);
    const auto result = store.load(100);
    EXPECT_EQ(result.metadata.step, 100U);
    EXPECT_DOUBLE_EQ(result.metadata.time, 0.5);
    EXPECT_EQ(result.values, values);
    ASSERT_TRUE(store.location(100).has_value());
    EXPECT_EQ(store.location(100)->offset, 8U);
    EXPECT_EQ(store.location(100)->size, 24U + values.size() * 8U);
    store.save(200, 1.0, {});
    EXPECT_EQ(store.location(200)->offset, 8U + store.location(100)->size);
    EXPECT_TRUE(store.load(200).values.empty());
}
TEST_F(StoreTest, ReopeningRebuildsIndexAndDuplicatesWin) {
    const std::vector<double> old_values{1, 2, 3};
    const std::vector<double> new_values{8, 9};
    {
        CheckpointStore store(path);
        store.save(300, 3, old_values);
        store.save(100, 1, old_values);
        const auto old_location = store.location(300);
        store.save(300, 30, new_values);
        EXPECT_GT(store.location(300)->offset, old_location->offset);
        EXPECT_EQ(store.size(), 2U);
        EXPECT_EQ(store.load(300).values, new_values);
    }
    {
        CheckpointStore store(path);
        EXPECT_EQ(store.size(), 2U);
        EXPECT_EQ(store.load(300).values, new_values);
        EXPECT_DOUBLE_EQ(store.load(300).metadata.time, 30);
        store.save(200, 2, {});
        EXPECT_EQ(store.size(), 3U);
    }
    CheckpointStore reopened(path);
    EXPECT_EQ(reopened.size(), 3U);
    EXPECT_EQ(reopened.range(100, 300),
              (std::vector<navier::storage::CheckpointMetadata>{{100, 1}, {200, 2}, {300, 30}}));
}
TEST_F(StoreTest, ManyCheckpointsAcrossLeafBoundaries) {
    {
        CheckpointStore store(path);
        const std::array<double, 2> values{1.25, 3.5};
        for (std::uint64_t step = 500; step > 0; --step) store.save(step * 10, static_cast<double>(step), values);
        EXPECT_EQ(store.size(), 500U);
        const auto range = store.range(205, 4505);
        ASSERT_EQ(range.size(), 430U);
        for (std::size_t i = 0; i < range.size(); ++i) {
            EXPECT_EQ(range[i].step, (i + 21) * 10);
            EXPECT_EQ(range[i].time, static_cast<double>(i + 21));
        }
        EXPECT_TRUE(store.range(51, 59).empty());
        EXPECT_TRUE(store.range(500, 400).empty());
        EXPECT_EQ(store.range(300, 300).size(), 1U);
    }
    CheckpointStore reopened(path);
    for (std::uint64_t step = 1; step <= 500; ++step)
        EXPECT_EQ(reopened.load(step * 10).values, (std::vector<double>{1.25, 3.5}));
}
TEST_F(StoreTest, Binary64BitsAndMaxStepRoundTrip) {
    const auto nan = std::bit_cast<double>(std::uint64_t{0x7ff8000000000042ULL});
    const std::vector<double> values{-0.0, std::numeric_limits<double>::infinity(), nan};
    const auto max_step = std::numeric_limits<std::uint64_t>::max();
    {
        CheckpointStore store(path);
        store.save(max_step, -0.0, values);
    }
    CheckpointStore store(path);
    const auto result = store.load(max_step);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(result.metadata.time), std::bit_cast<std::uint64_t>(-0.0));
    for (std::size_t i = 0; i < values.size(); ++i)
        EXPECT_EQ(std::bit_cast<std::uint64_t>(result.values[i]), std::bit_cast<std::uint64_t>(values[i]));
    EXPECT_EQ(store.range(max_step, max_step).size(), 1U);
}
TEST_F(StoreTest, FileUsesLittleEndianEncoding) {
    { CheckpointStore store(path); store.save(0x0102030405060708ULL, 1.0, {}); }
    std::ifstream in(path, std::ios::binary);
    in.seekg(8);
    std::array<unsigned char, 24> bytes{};
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    for (std::size_t i = 0; i < 8; ++i) EXPECT_EQ(bytes[i], 8 - i);
    EXPECT_EQ(bytes[14], 0xf0);
    EXPECT_EQ(bytes[15], 0x3f);
    for (std::size_t i = 16; i < 24; ++i) EXPECT_EQ(bytes[i], 0);
}
TEST_F(StoreTest, RejectsInvalidSignatureAndExistingEmptyFile) {
    { std::ofstream out(path, std::ios::binary); }
    EXPECT_THROW(CheckpointStore store(path), std::runtime_error);
    { std::ofstream out(path, std::ios::binary); out << "BADFILE!"; }
    EXPECT_THROW(CheckpointStore store(path), std::runtime_error);
    EXPECT_EQ(std::filesystem::file_size(path), 8U);
}
TEST_F(StoreTest, RejectsEveryTruncatedRecordBoundary) {
    const std::vector<double> values{1, 2};
    { CheckpointStore store(path); store.save(1, 1, values); }
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    for (std::size_t size = 9; size < bytes.size(); ++size) {
        { std::ofstream out(path, std::ios::binary | std::ios::trunc);
          out.write(bytes.data(), static_cast<std::streamsize>(size)); }
        EXPECT_THROW(CheckpointStore store(path), std::runtime_error) << "length " << size;
    }
}
TEST_F(StoreTest, RejectsOverflowingValueCount) {
    { CheckpointStore store(path); store.save(1, 1, {}); }
    { std::fstream out(path, std::ios::binary | std::ios::in | std::ios::out);
      out.seekp(24);
      const std::array<unsigned char, 8> bytes{255, 255, 255, 255, 255, 255, 255, 255};
      out.write(reinterpret_cast<const char*>(bytes.data()), 8); }
    EXPECT_THROW(CheckpointStore store(path), std::runtime_error);
}
TEST_F(StoreTest, InvalidParentPath) {
    EXPECT_THROW(CheckpointStore store(directory / "missing" / "sim.data"), std::runtime_error);
}
} // namespace
