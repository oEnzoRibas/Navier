#include "solvers/simulation2d.hpp"
#include "solvers/run_options.hpp"

int main(int argc, char** argv) {
    try {
        const auto run = navier::examples::options(argc, argv, "heat2d.data");
        navier::examples::Heat2D solver(run.side);
        navier::examples::describe(run, "heat2d", "[\"temperature\"]", solver.dt(), "zero-Dirichlet", solver.diffusivity());
        navier::storage::CheckpointStore store(run.path);
        store.save(0, 0, solver.values());
        for (std::uint64_t step = 1; step <= run.steps; ++step) {
            solver.advance();
            if (step % run.interval == 0 || step == run.steps)
                store.save(step, static_cast<double>(step) * solver.dt(), solver.values());
        }
        navier::examples::demonstrate(store, run);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Heat2D: " << error.what() << '\n';
        return 1;
    }
}
