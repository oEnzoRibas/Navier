#include "../examples/solvers/cavity2d.hpp"
#include <gtest/gtest.h>

namespace {
TEST(Cavity2D, StationaryWallsKeepExactRest) {
    navier::examples::Cavity2D solver(16, 100, 0);
    for (int i = 0; i < 20; ++i) solver.advance();
    for (const double value : solver.values()) EXPECT_EQ(value, 0);
    EXPECT_EQ(solver.max_divergence(), 0);
    EXPECT_EQ(solver.boundary_normal_max(), 0);
}
TEST(Cavity2D, ProjectionConservesIncompressibilityAndWallFlux) {
    navier::examples::Cavity2D solver(24);
    for (int step = 0; step < 500; ++step) {
        solver.advance();
        ASSERT_LT(solver.max_divergence(), 1.1e-6);
        ASSERT_EQ(solver.boundary_normal_max(), 0);
    }
    const auto values = solver.values();
    constexpr std::size_t cells = 24*24;
    bool return_flow = false;
    double mean_p = 0;
    for (std::size_t i = 0; i < cells; ++i) {
        EXPECT_TRUE(std::isfinite(values[i]));
        EXPECT_TRUE(std::isfinite(values[cells+i]));
        EXPECT_TRUE(std::isfinite(values[2*cells+i]));
        EXPECT_LT(std::hypot(values[i], values[cells+i]), 1.1);
        if (values[i] < -0.02) return_flow = true;
        mean_p += values[2*cells+i];
    }
    EXPECT_TRUE(return_flow);
    EXPECT_GT(values[(24-1)*24+12], 0.5); // Moving lid drives the upper interior.
    EXPECT_NEAR(mean_p / static_cast<double>(cells), 0, 1e-12);
}
TEST(Cavity2D, TimestepRespectsAdvectionAndDiffusionBounds) {
    for (const auto side : {16U, 32U, 64U}) {
        navier::examples::Cavity2D solver(side);
        const double h = 1.0 / side;
        EXPECT_LE(solver.dt() / h, 0.2 + 1e-14);
        EXPECT_LE(solver.viscosity()*solver.dt()/(h*h), 0.2 + 1e-14);
    }
    EXPECT_THROW(navier::examples::Cavity2D solver(16, -1), std::invalid_argument);
}
} // namespace

TEST(Cavity2DScaling, HighResolutionProjection) {
    for(const auto side : {128U,512U,1024U}) {
        navier::examples::Cavity2D solver(side);
        for(int step=0;step<3;++step)solver.advance();
        EXPECT_LT(solver.max_divergence(),1.1e-6) << "grid " << side;
        EXPECT_EQ(solver.boundary_normal_max(),0);
    }
}
