#include "solvers/cavity2d.hpp"
#include "solvers/run_options.hpp"

int main(int argc, char** argv) {
    try {
        auto run = navier::examples::options(argc, argv, "cavity2d.data");
        if (argc < 4) run.steps = 1000;
        if (argc < 5) run.interval = 60;
        navier::examples::Cavity2D solver(run.side);
        navier::examples::describe(run, "navier-stokes-cavity2d-Re100", "[\"velocity_x\", \"velocity_y\", \"pressure\"]",
                                  solver.dt(), "no-slip, top-lid-speed=1", solver.viscosity());
        navier::storage::CheckpointStore store(run.path);
        store.save(0, 0, solver.values());
        for (std::uint64_t step = 1; step <= run.steps; ++step) {
            solver.advance();
            if (step % run.interval == 0 || step == run.steps) {
                store.save(step, static_cast<double>(step) * solver.dt(), solver.values());
                if (step % 600 == 0 || step == run.steps)
                    std::cout << "step=" << step << " time=" << static_cast<double>(step)*solver.dt()
                              << " max_divergence=" << solver.max_divergence() << std::endl;
            }
        }
        navier::examples::demonstrate(store, run);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Navier-Stokes cavity: " << error.what() << '\n';
        return 1;
    }
}
