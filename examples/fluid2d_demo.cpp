#include "solvers/simulation2d.hpp"
#include "solvers/run_options.hpp"

int main(int argc, char** argv) {
    try {
        const auto run = navier::examples::options(argc, argv, "fluid2d.data");
        navier::examples::Fluid2D solver(run.side);
        navier::examples::describe(run, "fluid2d-d2q9", "[\"density\", \"velocity_x\", \"velocity_y\"]", 1, "periodic", 0.02);
        navier::storage::CheckpointStore store(run.path);
        store.save(0, 0, solver.values());
        for (std::uint64_t step = 1; step <= run.steps; ++step) {
            solver.advance();
            if (step % run.interval == 0 || step == run.steps)
                store.save(step, static_cast<double>(step), solver.values());
        }
        navier::examples::demonstrate(store, run);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fluid2D: " << error.what() << '\n';
        return 1;
    }
}
