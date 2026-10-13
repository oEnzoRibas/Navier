#include "solvers/antenna3d.hpp"
#include "solvers/run_options.hpp"
#include <navier/storage/dataset_store.hpp>
#include <sstream>

int main(int argc, char** argv) {
    try {
        auto run = navier::examples::options(argc, argv, "antenna3d.data");
        if (argc < 3) run.side = 48;
        if (argc < 4) run.steps = 600;
        if (argc < 5) run.interval = 6;
        const navier::examples::Antenna3D solver({.side = run.side});
        const auto& parameters = solver.options();
        std::ostringstream metadata;
        metadata.precision(17);
        metadata << "{\"schema_version\":1,\"solver\":\"antenna3d-retarded-dipole\","
                 << "\"numerical_method\":\"sampled analytical retarded Maxwell solution\","
                 << "\"boundary\":\"free-space observation window\","
                 << "\"spacing\":[" << solver.spacing() << ',' << solver.spacing() << ',' << solver.spacing() << "],"
                 << "\"origin\":[-8,-8,-8],"
                 << "\"parameters\":{\"dt\":" << parameters.dt << ",\"c\":1,\"epsilon\":1,\"mu\":1,"
                 << "\"period\":" << parameters.period << ",\"turn_on\":" << parameters.turn_on
                 << ",\"moment_amplitude\":" << parameters.moment << ",\"dipole_axis\":\"z\","
                 << "\"source_exclusion_radius\":" << parameters.exclusion_radius << "},"
                 << "\"potential_gauge\":\"Lorenz; E = -grad(phi) - dA/dt\","
                 << "\"radiation_energy_definition\":\"energy of the 1/r terms only, not full energy\","
                 << "\"masked_source\":\"NaN fields where source_mask=0\","
                 << "\"reference\":\"https://www.feynmanlectures.caltech.edu/II_21.html#Ch21-S4\"}";
        navier::storage::DatasetStore store(run.path, {.create = true, .metadata = metadata.str()});
        const std::array<const char*, navier::examples::Antenna3D::field_count> names{
            "electric_x", "electric_y", "electric_z", "magnetic_x", "magnetic_y", "magnetic_z",
            "poynting_x", "poynting_y", "poynting_z", "potential", "energy_density", "radiation_energy", "source_mask"};
        const std::vector<std::uint64_t> shape{run.side, run.side, run.side};
        const auto save = [&](std::uint64_t step) {
            const auto time = static_cast<double>(step) * parameters.dt;
            const auto snapshot = solver.snapshot(time);
            std::vector<navier::storage::FieldView> fields;
            for (std::size_t i = 0; i < names.size(); ++i)
                fields.push_back({{names[i], navier::storage::ScalarType::float32, shape,
                                   i == 12 ? "validity 0/1" : "normalized"}, std::span<const float>(snapshot[i])});
            double energy = 0, radiation = 0;
            for (std::size_t i = 0; i < snapshot[10].size(); ++i) {
                if (!snapshot[12][i]) continue;
                energy += snapshot[10][i]; radiation += snapshot[11][i];
            }
            const auto dv = std::pow(solver.spacing(), 3);
            const std::array<navier::storage::Metric, 3> metrics{{
                {"dipole_moment", solver.source(time).value, "normalized"},
                {"energy_in_window", energy * dv, "normalized"},
                {"radiation_energy_in_window", radiation * dv, "normalized"}}};
            store.save(step, time, fields, metrics);
        };
        save(0);
        // No integration over unsaved steps: the exact retarded solution is
        // sampled directly at checkpoint times, avoiding artificial dispersion.
        for (std::uint64_t step = run.interval; step <= run.steps; step += run.interval) save(step);
        if (run.steps % run.interval) save(run.steps);
        std::cout << "Antenna3D NAVIER02: " << store.size() << " analysis checkpoints in " << run.path.string()
                  << "\nIdeal dipole, retarded free-space fields; excluded source sphere r<0.6.\n"
                  << "Viewer: magnitude:electric / magnitude:magnetic + field lines; radiation_energy + volume.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Antenna3D: " << error.what() << '\n';
        return 1;
    }
}
