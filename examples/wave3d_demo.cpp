#include "solvers/run_options.hpp"
#include "solvers/wave3d.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        auto run =
            navier::examples::options(
                argc,
                argv,
                "wave3d.data"
            );

        // Defaults pensados para gerar algo bonito
        // sem ficar pesado demais.
        if (argc < 3) {
            run.side = 48;
        }

        if (argc < 4) {
            run.steps = 600;
        }

        if (argc < 5) {
            run.interval = 20;
        }

        constexpr double wave_speed = 1.0;
        constexpr double dt = 0.45;

        navier::examples::Wave3D solver(
            run.side,
            wave_speed,
            dt
        );

        auto sidecar = run.path;
        sidecar += ".json";

        if (std::filesystem::exists(sidecar)) {
            throw std::runtime_error(
                "Metadata output already exists"
            );
        }

        std::ofstream meta(sidecar);

        if (!meta) {
            throw std::runtime_error(
                "Cannot create wave metadata file"
            );
        }

        meta.precision(17);

        meta
            << "{\n"
            << "  \"solver\": \"wave3d-transient-pulse\",\n"
            << "  \"nx\": " << run.side << ",\n"
            << "  \"ny\": " << run.side << ",\n"
            << "  \"nz\": " << run.side << ",\n"
            << "  \"steps\": " << run.steps << ",\n"
            << "  \"save_every\": " << run.interval << ",\n"
            << "  \"dt\": " << solver.dt() << ",\n"
            << "  \"coefficient\": " << solver.wave_speed() << ",\n"
            << "  \"courant\": " << solver.courant_number() << ",\n"
            << "  \"boundary\": \"dirichlet\",\n"
            << "  \"layout\": "
               "\"planar-row-major, index=(z*ny+y)*nx+x\",\n"
            << "  \"fields\": ["
               "\"amplitude\", "
               "\"energy_density\""
               "]\n"
            << "}\n";

        meta.close();

        if (!meta) {
            throw std::runtime_error(
                "Cannot write wave metadata"
            );
        }

        navier::storage::CheckpointStore store(run.path);

        store.save(
            0,
            0.0,
            solver.values()
        );

        std::cout
            << "Navier transient wave simulation\n"
            << "Model: 3D scalar wave equation\n"
            << "Grid: "
            << run.side << 'x'
            << run.side << 'x'
            << run.side << '\n'
            << "Steps: " << run.steps << '\n'
            << "Checkpoint interval: " << run.interval << '\n'
            << "Wave speed: " << solver.wave_speed() << '\n'
            << "dt: " << solver.dt() << '\n'
            << "Courant number: " << solver.courant_number() << '\n';

        for (std::uint64_t step = 1;
             step <= run.steps;
             ++step) {

            solver.advance();

            if (
                step % run.interval == 0 ||
                step == run.steps
            ) {
                store.save(
                    step,
                    static_cast<double>(step) * solver.dt(),
                    solver.values()
                );

                std::cout
                    << "step=" << step
                    << " time=" << static_cast<double>(step) * solver.dt()
                    << " max|u|=" << solver.max_amplitude()
                    << " energy=" << solver.total_energy()
                    << '\n';
            }
        }

        navier::examples::demonstrate(store, run);

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "Wave3D: "
            << error.what()
            << '\n';
        return 1;
    }
}