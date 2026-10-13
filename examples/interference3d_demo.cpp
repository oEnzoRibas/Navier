#include "solvers/interference3d.hpp"
#include "solvers/run_options.hpp"
#include <navier/storage/dataset_store.hpp>
#include <sstream>

namespace {
double number(const char* input) {
    const std::string_view text(input);
    double value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value))
        throw std::invalid_argument("Phase/separation must be finite numbers");
    return value;
}
}
int main(int argc, char** argv) {
    try {
        if (argc > 7) throw std::invalid_argument("Usage: navier_interference3d [file] [side] [steps] [interval] [phase-degrees] [separation]");
        auto run = navier::examples::options(std::min(argc, 5), argv, "interference3d.data");
        if (argc < 3) run.side = 48;
        if (argc < 4) run.steps = 600;
        if (argc < 5) run.interval = 6;
        const auto phase = argc > 5 ? number(argv[5]) : 90;
        if (std::abs(phase) > 360) throw std::invalid_argument("Phase must be in [-360,360] degrees");
        const navier::examples::Interference3D solver({.side = run.side}, argc > 6 ? number(argv[6]) : 4,
                                                       phase * std::numbers::pi / 180);
        const auto& base = solver.first();
        std::ostringstream meta;
        meta.precision(17);
        meta << "{\"schema_version\":1,\"solver\":\"two-dipoles-interference3d\","
             << "\"numerical_method\":\"superposition of analytical retarded Maxwell fields\","
             << "\"boundary\":\"free-space observation window\",\"spacing\":["
             << base.spacing() << ',' << base.spacing() << ',' << base.spacing() << "],\"origin\":[-8,-8,-8],"
             << "\"parameters\":{\"dt\":0.05,\"period\":4,\"turn_on\":4,\"c\":1,\"epsilon\":1,\"mu\":1,"
             << "\"dipole_axes\":\"z,z\",\"separation\":" << solver.separation()
             << ",\"phase_degrees\":" << phase << ",\"source_exclusion_radius\":0.6},"
             << "\"interference_definition\":\"E1 dot E2 + B1 dot B2; signed cross energy\","
             << "\"potential_gauge\":\"Lorenz\",\"masked_source\":\"NaN inside either source sphere\"}";
        navier::storage::DatasetStore store(run.path, {.create = true, .metadata = meta.str()});
        const std::array<const char*, 16> names{"electric_x","electric_y","electric_z","magnetic_x","magnetic_y","magnetic_z",
            "poynting_x","poynting_y","poynting_z","potential","energy_density","radiation_energy","source_mask",
            "interference_energy","incoherent_energy","radiation_interference"};
        const std::vector<std::uint64_t> shape{run.side,run.side,run.side};
        const auto save = [&](std::uint64_t step) {
            const auto time = static_cast<double>(step) * base.options().dt;
            const auto data = solver.snapshot(time);
            std::vector<navier::storage::FieldView> fields;
            for (std::size_t i = 0; i < names.size(); ++i)
                fields.push_back({{names[i],navier::storage::ScalarType::float32,shape,i==12?"validity 0/1":"normalized"},std::span<const float>(data[i])});
            double energy = 0, cross = 0;
            for (std::size_t i = 0; i < data[12].size(); ++i) if (data[12][i]) { energy += data[10][i]; cross += data[13][i]; }
            const auto dv = std::pow(base.spacing(),3);
            const std::array<navier::storage::Metric,5> metrics{{
                {"dipole_a",base.source(time).value,"normalized"},{"dipole_b",solver.second().source(time).value,"normalized"},
                {"phase_degrees",phase,"degrees"},{"energy_in_window",energy*dv,"normalized"},{"interference_in_window",cross*dv,"normalized"}}};
            store.save(step,time,fields,metrics);
        };
        save(0);
        for (std::uint64_t step=run.interval;step<=run.steps;step+=run.interval) save(step);
        if (run.steps%run.interval) save(run.steps);
        std::cout << "Interference3D: " << store.size() << " NAVIER02 checkpoints; phase=" << phase
                  << " degrees, separation=" << solver.separation() << "\nViewer: electric_z or radiation_interference, diverging palette.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "Interference3D: " << error.what() << '\n'; return 1; }
}
