#include "solvers/fluid3d.hpp"
#include "solvers/run_options.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        auto run =
            navier::examples::options(argc, argv, "fluid3d.data");

        // RunOptions tem side=2048 por padrão,
        // mas Fluid3D aceita somente 8..64.
        if (argc < 3) {
            run.side = 64;
        }

        if (argc < 4) {
            run.steps = 10000;
        }

        if (argc < 5) {
            run.interval = 100;
        }

        constexpr double viscosity = 0.02;
        constexpr double amplitude = 0.035;

        navier::examples::Fluid3D solver(
            run.side,
            viscosity,
            amplitude
        );

        // Metadata 3D.
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
                "Cannot create 3D metadata file"
            );
        }

        meta.precision(17);

        meta
            << "{\n"
            << "  \"solver\": \"abc-flow3d-d3q19-bgk\",\n"
            << "  \"nx\": " << run.side << ",\n"
            << "  \"ny\": " << run.side << ",\n"
            << "  \"nz\": " << run.side << ",\n"
            << "  \"steps\": " << run.steps << ",\n"
            << "  \"save_every\": " << run.interval << ",\n"
            << "  \"dt\": 1,\n"
            << "  \"viscosity\": " << viscosity << ",\n"
            << "  \"amplitude\": " << amplitude << ",\n"
            << "  \"boundary\": \"periodic\",\n"
            << "  \"layout\": "
               "\"planar-row-major, index=(z*ny+y)*nx+x\",\n"
            << "  \"fields\": ["
               "\"density\", "
               "\"velocity_x\", "
               "\"velocity_y\", "
               "\"velocity_z\""
               "]\n"
            << "}\n";

        meta.close();

        if (!meta) {
            throw std::runtime_error(
                "Cannot write 3D metadata"
            );
        }

        navier::storage::CheckpointStore store(run.path);

        // Initial state.
        store.save(
            0,
            0.0,
            solver.values()
        );

        std::cout
            << "Navier 3D fluid simulation\n"
            << "Solver: D3Q19 BGK / ABC flow\n"
            << "Grid: "
            << run.side << 'x'
            << run.side << 'x'
            << run.side << '\n'
            << "Steps: " << run.steps << '\n'
            << "Checkpoint interval: "
            << run.interval << '\n'
            << "Viscosity: " << viscosity << '\n'
            << "Initial amplitude: " << amplitude << '\n';

        for (
            std::uint64_t step = 1;
            step <= run.steps;
            ++step
        ) {
            solver.advance();

            if (
                step % run.interval == 0 ||
                step == run.steps
            ) {
                const auto values = solver.values();

                store.save(
                    step,
                    static_cast<double>(step),
                    values
                );

                std::cout
                    << "Saved checkpoint step="
                    << step
                    << '\n';
            }
        }

        navier::examples::demonstrate(store, run);

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "Fluid3D: "
            << error.what()
            << '\n';

        return 1;
    }
}