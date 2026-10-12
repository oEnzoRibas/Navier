#include <navier/index/bplus_tree.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
struct CountingLess {
    std::uint64_t* comparisons;
    bool operator()(std::uint64_t a, std::uint64_t b) const { ++*comparisons; return a < b; }
};
struct LinearIndex {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> entries;
    std::uint64_t comparisons = 0;
    const std::uint64_t* find(std::uint64_t key) {
        for (const auto& entry : entries) {
            ++comparisons;
            if (entry.first == key) return &entry.second;
        }
        return nullptr;
    }
};
struct Measurement { double nanoseconds; std::uint64_t checksum; };
template <typename Index>
Measurement measure(Index& index, const std::vector<std::uint64_t>& queries) {
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (const auto key : queries)
        if (const auto* value = index.find(key)) checksum += *value + 1;
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return {std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(queries.size()), checksum};
}
std::size_t parse_size(std::string_view text) {
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0 || value > 10000000)
        throw std::invalid_argument("Arguments must be integers in [1, 10000000]");
    return value;
}
void benchmark(std::size_t count, std::size_t searches) {
    std::mt19937_64 random(0x4e4156494552ULL);
    std::vector<std::uint64_t> keys(count);
    std::iota(keys.begin(), keys.end(), std::uint64_t{0});
    std::shuffle(keys.begin(), keys.end(), random);
    std::uint64_t tree_comparisons = 0;
    navier::index::BPlusTree<std::uint64_t, std::uint64_t, 32, CountingLess> tree(CountingLess{&tree_comparisons});
    LinearIndex linear;
    linear.entries.reserve(count);
    for (const auto key : keys) { tree.insert(key, key); linear.entries.emplace_back(key, key); }
    if (!tree.validate()) throw std::runtime_error("Tree failed structural validation");
    std::uniform_int_distribution<std::uint64_t> distribution(0, static_cast<std::uint64_t>(count + count / 4));
    std::vector<std::uint64_t> queries(searches);
    for (auto& key : queries) key = distribution(random);
    // Warm the lookup paths before timing; query generation and construction are excluded.
    (void)measure(linear, queries);
    (void)measure(tree, queries);
    linear.comparisons = 0;
    tree_comparisons = 0;
    const auto linear_result = measure(linear, queries);
    const auto tree_result = measure(tree, queries);
    if (linear_result.checksum != tree_result.checksum) throw std::runtime_error("Lookup checksums disagree");
    std::cout << count << ',' << tree.height() << ',' << searches << ','
              << linear_result.nanoseconds << ',' << tree_result.nanoseconds << ','
              << static_cast<double>(linear.comparisons) / static_cast<double>(searches) << ','
              << static_cast<double>(tree_comparisons) / static_cast<double>(searches) << ','
              << tree_result.checksum << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 3) throw std::invalid_argument("Usage: navier_benchmark [elements] [lookups]");
        const auto searches = argc == 3 ? parse_size(argv[2]) : std::size_t{1000};
        std::cout << "elements,height,lookups,linear_ns_per_lookup,bplus_ns_per_lookup,linear_avg_comparisons,bplus_avg_comparisons,checksum\n";
        std::cout << std::fixed << std::setprecision(2);
        if (argc >= 2) benchmark(parse_size(argv[1]), searches);
        else for (const auto count : {1000U, 10000U, 100000U, 1000000U}) benchmark(count, searches);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Navier benchmark: " << error.what() << '\n';
        return 1;
    }
}
