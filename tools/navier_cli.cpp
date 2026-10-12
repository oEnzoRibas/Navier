#include <navier/navier.hpp>
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
void usage() {
    std::cout << "Navier checkpoint explorer\n\n"
              << "  navier info <file.data>\n"
              << "  navier get <file.data> <step>\n"
              << "  navier range <file.data> <begin-step> <end-step>\n\n"
              << "Range bounds are inclusive. Commands inspect existing files.\n";
}
std::uint64_t parse_step(std::string_view text) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw std::invalid_argument("Step must be an unsigned 64-bit integer");
    return value;
}
void print_metadata(navier::storage::CheckpointStore& store, const navier::storage::CheckpointMetadata& metadata) {
    const auto where = store.location(metadata.step).value();
    std::cout << "step=" << metadata.step << " time=" << metadata.time
              << " offset=" << where.offset << " bytes=" << where.size << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) { usage(); return 0; }
        const std::string_view command = argv[1];
        if (!((command == "info" && argc == 3) || (command == "get" && argc == 4) ||
              (command == "range" && argc == 5))) {
            usage();
            throw std::invalid_argument("Invalid command or argument count");
        }
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        if (command == "get" || command == "range") begin = parse_step(argv[3]);
        if (command == "range") end = parse_step(argv[4]);
        const std::filesystem::path path(argv[2]);
        if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Checkpoint file does not exist");
        navier::storage::CheckpointStore store(path);
        std::cout << std::setprecision(12);
        if (command == "info") {
            std::cout << "Navier checkpoint file: " << path.string() << "\nFile bytes: "
                      << std::filesystem::file_size(path) << "\nCheckpoints: " << store.size()
                      << "\nIndex: in-memory B+ Tree rebuilt from record headers\n";
            const auto entries = store.range(0, std::numeric_limits<std::uint64_t>::max());
            if (!entries.empty()) {
                std::cout << "Steps: " << entries.front().step << ".." << entries.back().step
                          << "\nFirst checkpoints:\n";
                const auto limit = std::min(entries.size(), std::size_t{10});
                for (std::size_t i = 0; i < limit; ++i) print_metadata(store, entries[i]);
                if (entries.size() > limit) std::cout << "... use range to list other checkpoints\n";
            }
        } else if (command == "get") {
            const auto checkpoint = store.load(begin);
            print_metadata(store, checkpoint.metadata);
            std::cout << "Values: " << checkpoint.values.size() << "\nPreview:";
            const auto limit = std::min(checkpoint.values.size(), std::size_t{8});
            for (std::size_t i = 0; i < limit; ++i) std::cout << ' ' << checkpoint.values[i];
            if (checkpoint.values.size() > limit) std::cout << " ...";
            std::cout << '\n';
        } else {
            const auto entries = store.range(begin, end);
            std::cout << "Range " << begin << ".." << end << "\nCheckpoints: " << entries.size() << '\n';
            for (const auto& metadata : entries) print_metadata(store, metadata);
        }
        return 0;
    } catch (const std::out_of_range& error) {
        std::cerr << "Navier: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Navier: " << error.what() << '\n';
        return 1;
    }
}
