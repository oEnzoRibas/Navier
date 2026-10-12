#include <navier/navier.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("Usage: navier_demo [data-file]");
        const std::filesystem::path path = argc == 2 ? argv[1] : "sim.data";
        navier::storage::CheckpointStore store(path);
        std::cout << "Navier synthetic checkpoint demo\n\n";
        for (std::uint64_t step = 0; step <= 600; step += 100) {
            std::vector<double> values;
            for (int i = 0; i < 8; ++i) values.push_back(static_cast<double>(step) * 0.01 + i);
            store.save(step, static_cast<double>(step) * 0.001, values);
            std::cout << "Saved checkpoint step=" << step << '\n';
        }
        const auto checkpoint = store.load(300);
        const auto where = store.location(300).value();
        std::cout << "\nQuerying step 300...\nFound:\n  offset: " << where.offset
                  << "\n  size: " << where.size << "\n  time: " << checkpoint.metadata.time << "\n  values:";
        for (const auto value : checkpoint.values) std::cout << ' ' << value;
        std::cout << "\n\nRange 200..500:\n";
        for (const auto metadata : store.range(200, 500)) std::cout << "  " << metadata.step << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Navier demo: " << error.what() << '\n';
        return 1;
    }
}
