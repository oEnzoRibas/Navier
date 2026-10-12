#include "../examples/solvers/simulation2d.hpp"
#include <gtest/gtest.h>
#include <numeric>

namespace {
TEST(Heat2D, MaximumPrincipleAndExactDiscreteMode) {
    constexpr std::size_t side = 32;
    navier::examples::Heat2D solver(side);
    const auto initial = solver.values();
    const double decay = 1 - 1.6 * std::pow(std::sin(std::numbers::pi / (2 * static_cast<double>(side - 1))), 2);
    double old_peak = *std::max_element(initial.begin(), initial.end());
    for (int step = 1; step <= 200; ++step) {
        solver.advance();
        const auto& values = solver.values();
        const double peak = *std::max_element(values.begin(), values.end());
        EXPECT_LT(peak, old_peak);
        old_peak = peak;
        for (std::size_t i = 0; i < values.size(); ++i) {
            EXPECT_GE(values[i], 0);
            EXPECT_NEAR(values[i], initial[i] * std::pow(decay, step), 1e-12);
        }
    }
    for (std::size_t i = 0; i < side; ++i) {
        EXPECT_EQ(solver.values()[i], 0);
        EXPECT_EQ(solver.values()[(side - 1) * side + i], 0);
        EXPECT_EQ(solver.values()[i * side], 0);
        EXPECT_EQ(solver.values()[i * side + side - 1], 0);
    }
}
TEST(Heat2D, ContinuumSolutionApproximated) {
    navier::examples::Heat2D solver(64);
    const auto initial = solver.values();
    for (int step = 0; step < 400; ++step) solver.advance();
    const double exact_decay = std::exp(-2 * std::numbers::pi * std::numbers::pi * solver.diffusivity() * 400 * solver.dt());
    for (std::size_t i = 0; i < initial.size(); ++i)
        EXPECT_NEAR(solver.values()[i], initial[i] * exact_decay, 1e-4);
}
TEST(Fluid2D, RestEquilibriumRemainsAtRest) {
    navier::examples::Fluid2D solver(16, 0.02, 0);
    for (int step = 0; step < 100; ++step) solver.advance();
    const auto values = solver.values();
    for (std::size_t i = 0; i < 256; ++i) {
        EXPECT_NEAR(values[i], 1, 1e-13);
        EXPECT_NEAR(values[256 + i], 0, 1e-13);
        EXPECT_NEAR(values[512 + i], 0, 1e-13);
    }
}
double energy(const std::vector<double>& values) {
    const auto cells = values.size() / 3;
    double total = 0;
    for (std::size_t i = 0; i < cells; ++i)
        total += 0.5 * values[i] * (values[cells + i] * values[cells + i] + values[2 * cells + i] * values[2 * cells + i]);
    return total;
}
TEST(Fluid2D, PeriodicVortexConservesMassAndDissipatesEnergy) {
    constexpr std::size_t cells = 32 * 32;
    navier::examples::Fluid2D solver(32);
    const auto initial = solver.values();
    const double mass = std::accumulate(initial.begin(), initial.begin() + cells, 0.0);
    for (int step = 0; step < 400; ++step) {
        solver.advance();
        if (step % 20 != 0) continue;
        const auto values = solver.values();
        EXPECT_NEAR(std::accumulate(values.begin(), values.begin() + cells, 0.0), mass, mass * 1e-11);
        double momentum_x = 0;
        double momentum_y = 0;
        for (std::size_t i = 0; i < cells; ++i) {
            EXPECT_TRUE(std::isfinite(values[i]));
            EXPECT_GT(values[i], 0.99);
            EXPECT_LT(values[i], 1.01);
            momentum_x += values[i] * values[cells + i];
            momentum_y += values[i] * values[2 * cells + i];
        }
        EXPECT_NEAR(momentum_x, 0, 1e-10);
        EXPECT_NEAR(momentum_y, 0, 1e-10);
    }
    const double final_energy = energy(solver.values());
    EXPECT_LT(final_energy, energy(initial));
    const double k = 2 * std::numbers::pi / 32;
    const double expected = energy(initial) * std::exp(-4 * 0.02 * k * k * 400);
    EXPECT_NEAR(final_energy, expected, expected * 0.02);
}
TEST(Simulations2D, RejectInvalidParameters) {
    EXPECT_THROW(navier::examples::Heat2D solver(1), std::invalid_argument);
    EXPECT_THROW(navier::examples::Heat2D solver(16, -1), std::invalid_argument);
    EXPECT_THROW(navier::examples::Fluid2D solver(513), std::invalid_argument);
    EXPECT_THROW(navier::examples::Fluid2D solver(16, 0), std::invalid_argument);
    EXPECT_THROW(navier::examples::Fluid2D solver(16, 0.02, 1), std::invalid_argument);
}
} // namespace
