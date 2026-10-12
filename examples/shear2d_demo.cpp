#include "solvers/simulation2d.hpp"
#include "solvers/run_options.hpp"
#include <numeric>
int main(int argc, char** argv) {
    try {
        auto run = navier::examples::options(argc, argv, "shear2d.data");
        if (argc < 3) run.side = 192;
        if (argc < 4) run.steps = 4000;
        if (argc < 5) run.interval = 80;
        navier::examples::Fluid2D solver(run.side, 0.02, 0.08, navier::examples::FlowPattern::ShearLayer);
        navier::examples::describe(run, "kelvin-helmholtz-shear2d", "[\"density\", \"velocity_x\", \"velocity_y\"]",
                                  1, "periodic double shear layer", 0.02);
        navier::storage::CheckpointStore store(run.path);
        store.save(0, 0, solver.values());
        for (std::uint64_t step=1; step<=run.steps; ++step) {
            solver.advance();
            if (step % run.interval == 0 || step == run.steps) {
                const auto values = solver.values();
                store.save(step, static_cast<double>(step), values);
                if (step % 400 == 0 || step == run.steps) {
                    const auto cells=run.side*run.side;
                    const double mass=std::accumulate(values.begin(), values.begin()+static_cast<std::ptrdiff_t>(cells), 0.0);
                    std::cout << "step=" << step << " mass=" << mass << std::endl;
                }
            }
        }
        navier::examples::demonstrate(store, run);
        return 0;
    } catch(const std::exception& error) { std::cerr << "Shear2D: " << error.what() << '\n'; return 1; }
}
