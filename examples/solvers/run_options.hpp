#pragma once
#include <navier/navier.hpp>
#include <charconv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace navier::examples {
struct RunOptions {
    std::filesystem::path path;
    std::size_t side = 64;
    std::uint64_t steps = 400;
    std::uint64_t interval = 20;
};
inline std::uint64_t argument(std::string_view text) {
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc{} || end != text.data() + text.size() || result == 0 || result > 1000000)
        throw std::invalid_argument("Numeric arguments must be in [1, 1000000]");
    return result;
}
inline RunOptions options(int argc, char** argv, const char* default_file) {
    if (argc > 5) throw std::invalid_argument("Usage: simulation [data-file] [grid-side] [steps] [save-every]");
    RunOptions result;
    result.path = argc > 1 ? argv[1] : default_file;
    if (argc > 2) result.side = static_cast<std::size_t>(argument(argv[2]));
    if (argc > 3) result.steps = argument(argv[3]);
    if (argc > 4) result.interval = argument(argv[4]);
    if (std::filesystem::exists(result.path))
        throw std::runtime_error("Simulation output already exists; choose a new data-file path");
    return result;
}
// A small sidecar describes field planes without introducing physics into the store format.
inline void describe(const RunOptions& run, const char* solver, const char* fields, double dt,
                     const char* boundary, double coefficient) {
    auto path = run.path;
    path += ".json";
    if (std::filesystem::exists(path)) throw std::runtime_error("Metadata output already exists");
    std::ofstream out(path);
    out.precision(17);
    out << "{\n  \"solver\": \"" << solver << "\",\n  \"nx\": " << run.side
        << ",\n  \"ny\": " << run.side << ",\n  \"steps\": " << run.steps
        << ",\n  \"save_every\": " << run.interval << ",\n  \"dt\": " << dt
        << ",\n  \"coefficient\": " << coefficient << ",\n  \"boundary\": \"" << boundary
        << "\",\n  \"layout\": \"planar-row-major, index=y*nx+x\",\n  \"fields\": " << fields << "\n}\n";
    out.close();
    if (!out) throw std::runtime_error("Cannot write simulation metadata");
}
inline void demonstrate(navier::storage::CheckpointStore& store, const RunOptions& run) {
    const auto final = store.load(run.steps);
    std::cout << "Saved " << store.size() << " checkpoints to " << run.path.string()
              << "\nFinal step=" << final.metadata.step << " time=" << final.metadata.time
              << " payload_values=" << final.values.size() << '\n';
    std::cout << "Steps in final interval:";
    for (const auto metadata : store.range(run.steps / 2, run.steps)) std::cout << ' ' << metadata.step;
    std::cout << '\n';
}
} // namespace navier::examples
