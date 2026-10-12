#include <navier/index/bplus_tree.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {
template <typename Order>
class TreeTest : public testing::Test {
protected:
    navier::index::BPlusTree<int, int, Order::value> tree;
    void check_sequence(std::vector<int> keys) {
        std::map<int, int> oracle;
        for (int key : keys) {
            ASSERT_TRUE(tree.insert(key, key * 7));
            oracle[key] = key * 7;
            ASSERT_TRUE(tree.validate()) << "after key " << key;
            ASSERT_EQ(tree.size(), oracle.size());
        }
        for (const auto& [key, value] : oracle) {
            ASSERT_NE(tree.find(key), nullptr);
            EXPECT_EQ(*tree.find(key), value);
        }
        EXPECT_EQ(tree.find(-1), nullptr);
        EXPECT_EQ(tree.find(static_cast<int>(keys.size())), nullptr);
        std::vector<std::pair<int, int>> expected(oracle.begin(), oracle.end());
        EXPECT_EQ(tree.range(-1, static_cast<int>(keys.size())), expected);
        EXPECT_GE(tree.height(), 3U);
    }
};
using Orders = testing::Types<std::integral_constant<std::size_t, 3>,
    std::integral_constant<std::size_t, 4>, std::integral_constant<std::size_t, 5>,
    std::integral_constant<std::size_t, 32>, std::integral_constant<std::size_t, 64>>;
TYPED_TEST_SUITE(TreeTest, Orders);

TYPED_TEST(TreeTest, Empty) {
    EXPECT_TRUE(this->tree.empty());
    EXPECT_EQ(this->tree.size(), 0U);
    EXPECT_EQ(this->tree.height(), 1U);
    EXPECT_EQ(this->tree.find(7), nullptr);
    EXPECT_FALSE(this->tree.contains(7));
    EXPECT_TRUE(this->tree.range(0, 10).empty());
    EXPECT_TRUE(this->tree.validate());
}
TYPED_TEST(TreeTest, SingleAndDuplicate) {
    EXPECT_TRUE(this->tree.insert(7, 14));
    EXPECT_FALSE(this->tree.empty());
    ASSERT_NE(this->tree.find(7), nullptr);
    EXPECT_EQ(*this->tree.find(7), 14);
    EXPECT_TRUE(this->tree.contains(7));
    EXPECT_FALSE(this->tree.insert(7, 21));
    EXPECT_EQ(*this->tree.find(7), 21);
    EXPECT_EQ(this->tree.size(), 1U);
    EXPECT_TRUE(this->tree.validate());
}
TYPED_TEST(TreeTest, FirstLeafSplitCreatesRoot) {
    for (std::size_t i = 0; i < TypeParam::value - 1; ++i)
        this->tree.insert(static_cast<int>(i), static_cast<int>(i));
    EXPECT_EQ(this->tree.height(), 1U);
    this->tree.insert(static_cast<int>(TypeParam::value - 1), 42);
    EXPECT_EQ(this->tree.height(), 2U);
    EXPECT_TRUE(this->tree.validate());
    for (std::size_t i = 0; i < TypeParam::value; ++i)
        EXPECT_TRUE(this->tree.contains(static_cast<int>(i)));
}
TYPED_TEST(TreeTest, AscendingMultipleLevels) {
    std::vector<int> keys(5000);
    std::iota(keys.begin(), keys.end(), 0);
    this->check_sequence(keys);
}
TYPED_TEST(TreeTest, DescendingMultipleLevels) {
    std::vector<int> keys(5000);
    std::iota(keys.rbegin(), keys.rend(), 0);
    this->check_sequence(keys);
}
TYPED_TEST(TreeTest, RandomInsertionAndRangeOracle) {
    std::vector<int> keys(5000);
    std::iota(keys.begin(), keys.end(), 0);
    std::mt19937 generator(0x4e415649U);
    std::shuffle(keys.begin(), keys.end(), generator);
    this->check_sequence(keys);
    std::map<int, int> oracle;
    for (int key : keys) oracle[key] = key * 7;
    std::uniform_int_distribution<int> bounds(-100, 5100);
    for (int query = 0; query < 200; ++query) {
        const int begin = bounds(generator);
        const int end = bounds(generator);
        std::vector<std::pair<int, int>> expected;
        if (begin <= end)
            for (auto it = oracle.lower_bound(begin); it != oracle.end() && it->first <= end; ++it)
                expected.push_back(*it);
        EXPECT_EQ(this->tree.range(begin, end), expected);
    }
    for (int key : keys) EXPECT_FALSE(this->tree.insert(key, -key));
    EXPECT_EQ(this->tree.size(), 5000U);
    EXPECT_TRUE(this->tree.validate());
    for (int key : keys) EXPECT_EQ(*this->tree.find(key), -key);
}
TYPED_TEST(TreeTest, RangesAndGaps) {
    for (int key = 0; key < 200; key += 2) this->tree.insert(key, key + 1);
    using Pair = std::pair<int, int>;
    EXPECT_EQ(this->tree.range(4, 6), (std::vector<Pair>{{4, 5}, {6, 7}}));
    EXPECT_EQ(this->tree.range(4, 4), (std::vector<Pair>{{4, 5}}));
    EXPECT_TRUE(this->tree.range(5, 5).empty());
    EXPECT_TRUE(this->tree.range(50, 20).empty());
    EXPECT_TRUE(this->tree.range(-10, -1).empty());
    EXPECT_TRUE(this->tree.range(200, 300).empty());
    std::vector<Pair> expected;
    for (int key = 10; key <= 180; key += 2) expected.emplace_back(key, key + 1);
    EXPECT_EQ(this->tree.range(9, 181), expected);
    EXPECT_TRUE(this->tree.validate());
}
TEST(BPlusTree, DescendingComparator) {
    navier::index::BPlusTree<int, std::string, 3, std::greater<int>> tree;
    for (int i = 0; i < 200; ++i) tree.insert(i, std::to_string(i));
    ASSERT_TRUE(tree.validate());
    EXPECT_TRUE(tree.range(0, 10).empty());
    const auto range = tree.range(180, 20);
    ASSERT_EQ(range.size(), 161U);
    for (std::size_t i = 0; i < range.size(); ++i)
        EXPECT_EQ(range[i].first, 180 - static_cast<int>(i));
    for (int i = 0; i < 200; ++i) EXPECT_EQ(*tree.find(i), std::to_string(i));
}
struct DecadeCompare {
    bool operator()(int a, int b) const { return a / 10 < b / 10; }
};
TEST(BPlusTree, ComparatorEquivalenceDefinesDuplicates) {
    navier::index::BPlusTree<int, int, 3, DecadeCompare> tree;
    for (int i = 0; i < 100; i += 10) tree.insert(i, i);
    EXPECT_FALSE(tree.insert(29, 999));
    EXPECT_EQ(tree.size(), 10U);
    ASSERT_NE(tree.find(21), nullptr);
    EXPECT_EQ(*tree.find(21), 999);
    EXPECT_EQ(tree.range(21, 29).size(), 1U);
    EXPECT_TRUE(tree.validate());
}
TEST(BPlusTree, UnsignedBoundaryKeys) {
    navier::index::BPlusTree<std::uint64_t, int, 3> tree;
    const auto max = std::numeric_limits<std::uint64_t>::max();
    for (const auto key : {std::uint64_t{0}, max, max - 1, std::uint64_t{1}}) tree.insert(key, 42);
    EXPECT_TRUE(tree.validate());
    EXPECT_EQ(tree.range(0, max).size(), 4U);
    EXPECT_EQ(tree.range(max, max).size(), 1U);
    EXPECT_EQ(*tree.find(max), 42);
}
TEST(BPlusTree, StringKeysAndNonDefaultValues) {
    struct Value { explicit Value(int number) : n(number) {} int n; };
    navier::index::BPlusTree<std::string, Value, 3> tree;
    for (const std::string key : {"zeta", "alpha", "gamma", "beta", "delta"}) tree.insert(key, Value(5));
    EXPECT_TRUE(tree.validate());
    EXPECT_EQ(tree.find("beta")->n, 5);
    EXPECT_EQ(tree.range("beta", "gamma").size(), 3U);
}
} // namespace
