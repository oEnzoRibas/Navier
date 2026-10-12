#include "solvers/taylor_green3d.hpp"
#include "solvers/run_options.hpp"

#include <cstdint>
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
                "taylor_green3d.data"
            );

        if (argc < 3)
            run.side = 64;

        if (argc < 4)
            run.steps = 10000;

        if (argc < 5)
            run.interval = 100;

        constexpr double reynolds = 1600.0;
        constexpr double initial_velocity = 0.05;

        navier::examples::TaylorGreen3D solver(
            run.side,
            reynolds,
            initial_velocity
        );

        auto sidecar = run.path;
        sidecar += ".json";

        if (std::filesystem::exists(sidecar)) {
            throw std::runtime_error(
                "Metadata output already exists"
            );
        }

        std::ofstream meta(sidecar);

        meta
            << "{"
            << "\"solver\":\"taylor-green-vortex3d-d3q19\","
            << "\"nx\":" << run.side << ","
            << "\"ny\":" << run.side << ","
            << "\"nz\":" << run.side << ","
            << "\"dt\":1,"
            << "\"steps\":" << run.steps << ","
            << "\"save_every\":" << run.interval << ","
            << "\"reynolds\":" << reynolds << ","
            << "\"initial_velocity\":" << initial_velocity << ","
            << "\"boundary\":\"periodic\","
            << "\"layout\":\"planar, index=(z*ny+y)*nx+x\","
            << "\"fields\":["
            << "\"density\","
            << "\"velocity_x\","
            << "\"velocity_y\","
            << "\"velocity_z\","
            << "\"vorticity_magnitude\""
            << "]"
            << "}";

        meta.close();

        if (!meta) {
            throw std::runtime_error(
                "Cannot write Taylor-Green metadata"
            );
        }

        navier::storage::CheckpointStore store(run.path);

        store.save(
            0,
            0.0,
            solver.values()
        );

        std::cout
            << "Taylor-Green vortex simulation\n"
            << "Grid: "
            << run.side << "^3\n"
            << "Re: "
            << reynolds << '\n'
            << "Steps: "
            << run.steps << '\n';

        for (std::uint64_t step = 1;
             step <= run.steps;
             ++step) {

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

                if (
                    step % 100 == 0 ||
                    step == run.steps
                ) {
                    std::cout
                        << "step=" << step
                        << " kinetic_energy="
                        << solver.kinetic_energy()
                        << " max_vorticity="
                        << solver.max_vorticity()
                        << '\n';
                }
            }
        }

        navier::examples::demonstrate(
            store,
            run
        );

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "TaylorGreen3D: "
            << error.what()
            << '\n';

        return 1;
    }
}