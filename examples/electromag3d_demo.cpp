#include "solvers/electromag3d.hpp"
#include "solvers/run_options.hpp"

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
                "electromag3d.data"
            );

        // Defaults pensados para caber no viewer.
        if (argc < 3) {
            run.side = 40;
        }

        if (argc < 4) {
            run.steps = 500;
        }

        if (argc < 5) {
            run.interval = 10;
        }

        navier::examples::Electromagnetic3D solver(
            run.side,
            0.5
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
                "Cannot create metadata file"
            );
        }

        meta.precision(17);

        meta
            << "{\n"
            << "  \"solver\": \"em3d-fdtd-pec-cavity\",\n"
            << "  \"nx\": " << run.side << ",\n"
            << "  \"ny\": " << run.side << ",\n"
            << "  \"nz\": " << run.side << ",\n"
            << "  \"steps\": " << run.steps << ",\n"
            << "  \"save_every\": " << run.interval << ",\n"
            << "  \"dt\": " << solver.dt() << ",\n"
            << "  \"coefficient\": " << solver.dt() << ",\n"
            << "  \"boundary\": \"pec\",\n"
            << "  \"layout\": "
               "\"planar-row-major, index=(z*ny+y)*nx+x\",\n"
            << "  \"fields\": ["
               "\"energy_density\", "
               "\"velocity_x\", "
               "\"velocity_y\", "
               "\"velocity_z\", "
               "\"electric_magnitude\""
               "]\n"
            << "}\n";

        meta.close();

        if (!meta) {
            throw std::runtime_error(
                "Cannot write metadata"
            );
        }

        navier::storage::CheckpointStore store(run.path);

        store.save(
            0,
            0.0,
            solver.values()
        );

        std::cout
            << "Navier electromagnetic 3D simulation\n"
            << "Model: EM pulse in PEC cavity\n"
            << "Grid: "
            << run.side << 'x'
            << run.side << 'x'
            << run.side << '\n'
            << "Steps: " << run.steps << '\n'
            << "Checkpoint interval: " << run.interval << '\n'
            << "dt: " << solver.dt() << '\n';

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
                    << " energy=" << solver.total_energy()
                    << " max|E|=" << solver.max_electric_field()
                    << '\n';
            }
        }

        navier::examples::demonstrate(store, run);

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "Electromagnetic3D: "
            << error.what()
            << '\n';
        return 1;
    }
}