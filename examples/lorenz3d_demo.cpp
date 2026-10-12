#include "solvers/lorenz3d.hpp"
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
                "lorenz3d.data"
            );

        if (argc < 3) {
            run.side = 128;
        }

        if (argc < 4) {
            run.steps = 20000;
        }

        if (argc < 5) {
            run.interval = 200;
        }

        constexpr double dt = 0.005;
        constexpr double sigma = 10.0;
        constexpr double rho = 28.0;
        constexpr double beta = 8.0 / 3.0;

        navier::examples::Lorenz3D solver(
            run.side,
            dt,
            sigma,
            rho,
            beta
        );

        auto metadataPath = run.path;
        metadataPath += ".json";

        if (std::filesystem::exists(metadataPath)) {
            throw std::runtime_error(
                "Metadata output already exists"
            );
        }

        std::ofstream meta(metadataPath);

        if (!meta) {
            throw std::runtime_error(
                "Cannot create Lorenz metadata"
            );
        }

        meta.precision(17);

        meta
            << "{\n"
            << "  \"solver\": \"lorenz-attractor-rk4\",\n"
            << "  \"nx\": " << run.side << ",\n"
            << "  \"ny\": " << run.side << ",\n"
            << "  \"nz\": " << run.side << ",\n"
            << "  \"steps\": " << run.steps << ",\n"
            << "  \"save_every\": " << run.interval << ",\n"
            << "  \"dt\": " << dt << ",\n"
            << "  \"sigma\": " << sigma << ",\n"
            << "  \"rho\": " << rho << ",\n"
            << "  \"beta\": " << beta << ",\n"
            << "  \"boundary\": \"phase-space\",\n"
            << "  \"layout\": "
               "\"planar-row-major, index=(z*ny+y)*nx+x\",\n"
            << "  \"phase_space\": {\n"
            << "    \"x\": [-30, 30],\n"
            << "    \"y\": [-30, 30],\n"
            << "    \"z\": [0, 60]\n"
            << "  },\n"
            << "  \"fields\": [\"trajectory_density\"]\n"
            << "}\n";

        meta.close();

        if (!meta) {
            throw std::runtime_error(
                "Cannot write Lorenz metadata"
            );
        }

        navier::storage::CheckpointStore store(
            run.path
        );

        store.save(
            0,
            0.0,
            solver.values()
        );

        std::cout
            << "Navier Lorenz attractor\n"
            << "Integrator: RK4\n"
            << "Grid: "
            << run.side << '^' << 3 << '\n'
            << "Steps: "
            << run.steps << '\n'
            << "Save every: "
            << run.interval << '\n'
            << "dt: " << dt << '\n'
            << "sigma: " << sigma << '\n'
            << "rho: " << rho << '\n'
            << "beta: " << beta << '\n';

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
                store.save(
                    step,
                    static_cast<double>(step) * dt,
                    solver.values()
                );

                std::cout
                    << "step=" << step
                    << " t=" << step * dt
                    << " state=("
                    << solver.x() << ", "
                    << solver.y() << ", "
                    << solver.z() << ")\n";
            }
        }

        navier::examples::demonstrate(
            store,
            run
        );

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "Lorenz3D: "
            << error.what()
            << '\n';

        return 1;
    }
}