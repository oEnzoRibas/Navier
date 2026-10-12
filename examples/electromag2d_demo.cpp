#include "solvers/electromag2d.hpp"
#include "solvers/run_options.hpp"

#include <cstdint>
#include <iostream>

int main(int argc, char** argv) {
    try {
        auto run =
            navier::examples::options(
                argc,
                argv,
                "electromag2d.data"
            );

        // Good defaults for the electromagnetic example.
        if (argc < 3) {
            run.side = 256;
        }

        if (argc < 4) {
            run.steps = 1200;
        }

        if (argc < 5) {
            run.interval = 20;
        }

        navier::examples::Electromagnetic2D solver(
            run.side
        );

        navier::examples::describe(
            run,
            "fdtd2d-tmz-gaussian-pulse",
            R"(["electric_z","magnetic_x","magnetic_y"])",
            solver.dt(),
            "pec",
            solver.courant_number()
        );

        navier::storage::CheckpointStore store(
            run.path
        );

        // Initial state.
        store.save(
            0,
            0.0,
            solver.values()
        );

        std::cout
            << "Navier electromagnetic simulation\n"
            << "Method: FDTD / TMz\n"
            << "Grid: "
            << run.side
            << 'x'
            << run.side
            << '\n'
            << "Steps: "
            << run.steps
            << '\n'
            << "Checkpoint interval: "
            << run.interval
            << '\n'
            << "dt: "
            << solver.dt()
            << '\n'
            << "Courant number: "
            << solver.courant_number()
            << '\n'
            << "Boundary: PEC\n";

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
                    static_cast<double>(step)
                        * solver.dt(),
                    solver.values()
                );

                std::cout
                    << "step="
                    << step
                    << " time="
                    << static_cast<double>(step)
                       * solver.dt()
                    << " max|Ez|="
                    << solver.max_electric_field()
                    << " energy="
                    << solver.electromagnetic_energy()
                    << '\n';
            }
        }

        navier::examples::demonstrate(
            store,
            run
        );

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "Electromagnetic2D: "
            << error.what()
            << '\n';

        return 1;
    }
}