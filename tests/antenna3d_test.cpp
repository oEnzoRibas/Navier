#include "../examples/solvers/antenna3d.hpp"
#include <gtest/gtest.h>

using navier::examples::Antenna3D;

TEST(Antenna3D,SourceIsCausalAndFieldsArriveAtLightSpeed) {
    const Antenna3D solver;
    const auto before = solver.sample(4, 0, 0, 3.99);
    for (const auto value : before.electric) EXPECT_EQ(value, 0);
    for (const auto value : before.magnetic) EXPECT_EQ(value, 0);
    EXPECT_EQ(before.energy, 0);
    EXPECT_GT(solver.sample(4, 0, 0, 5).energy, 0);
    EXPECT_EQ(solver.source(-1).value, 0);
}

TEST(Antenna3D,RetardedPotentialsReproduceElectricAndMagneticFields) {
    const Antenna3D solver;
    constexpr double h = 1e-4, time = 4.7;
    const std::array<double, 3> point{2.3, 1.4, -1.1};
    const auto center = solver.sample(point[0], point[1], point[2], time);
    std::array<std::array<double, 3>, 3> derivatives{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        auto plus = point, minus = point;
        plus[axis] += h; minus[axis] -= h;
        const auto a = solver.sample(plus[0], plus[1], plus[2], time);
        const auto b = solver.sample(minus[0], minus[1], minus[2], time);
        const auto next = solver.sample(point[0], point[1], point[2], time + h);
        const auto previous = solver.sample(point[0], point[1], point[2], time - h);
        const auto field = -(a.potential - b.potential) / (2 * h) -
                           (next.vector_potential[axis] - previous.vector_potential[axis]) / (2 * h);
        EXPECT_NEAR(center.electric[axis], field, 1e-9);
        for (std::size_t component = 0; component < 3; ++component)
            derivatives[axis][component] = (a.vector_potential[component] - b.vector_potential[component]) / (2 * h);
    }
    EXPECT_NEAR(center.magnetic[0], derivatives[1][2] - derivatives[2][1], 1e-9);
    EXPECT_NEAR(center.magnetic[1], derivatives[2][0] - derivatives[0][2], 1e-9);
    EXPECT_NEAR(center.magnetic[2], derivatives[0][1] - derivatives[1][0], 1e-9);
}

TEST(Antenna3D,VacuumMaxwellResidualsOutsideSource) {
    const Antenna3D solver;
    constexpr double h = 1e-4, time = 9.3;
    const std::array<double, 3> point{2.3, 1.4, -1.1};
    std::array<std::array<double, 3>, 3> de{}, db{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        auto plus = point, minus = point;
        plus[axis] += h; minus[axis] -= h;
        const auto a = solver.sample(plus[0], plus[1], plus[2], time);
        const auto b = solver.sample(minus[0], minus[1], minus[2], time);
        for (std::size_t component = 0; component < 3; ++component) {
            de[axis][component] = (a.electric[component] - b.electric[component]) / (2 * h);
            db[axis][component] = (a.magnetic[component] - b.magnetic[component]) / (2 * h);
        }
    }
    EXPECT_NEAR(de[0][0] + de[1][1] + de[2][2], 0, 1e-9);
    EXPECT_NEAR(db[0][0] + db[1][1] + db[2][2], 0, 1e-9);
    const auto next = solver.sample(point[0], point[1], point[2], time + h);
    const auto previous = solver.sample(point[0], point[1], point[2], time - h);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto j = (axis + 1) % 3, k = (axis + 2) % 3;
        EXPECT_NEAR(de[j][k] - de[k][j], -(next.magnetic[axis] - previous.magnetic[axis]) / (2 * h), 1e-9);
        EXPECT_NEAR(db[j][k] - db[k][j], (next.electric[axis] - previous.electric[axis]) / (2 * h), 1e-9);
    }
}

TEST(Antenna3D,FarFieldIsTransverseOutwardAndHasDipoleAngularPattern) {
    const Antenna3D solver;
    // Compare at equal retarded phase; steady source sin(omega*t)=1.
    const auto equator = solver.sample(1000, 0, 0, 1009);
    const auto pole = solver.sample(0, 0, 1000, 1009);
    EXPECT_EQ(pole.radiation_energy, 0);
    EXPECT_GT(equator.radiation_energy, 0);
    EXPECT_EQ(equator.electric[0], 0);
    EXPECT_NEAR(equator.electric[2], -equator.magnetic[1], std::abs(equator.magnetic[1]) * 1e-5);
    EXPECT_GT(equator.poynting[0], 0);
    constexpr double theta = 0.7, r = 1000;
    const auto angled = solver.sample(r * std::sin(theta), 0, r * std::cos(theta), 1009);
    EXPECT_NEAR(angled.radiation_energy / equator.radiation_energy, std::pow(std::sin(theta), 2), 1e-10);
    const auto twice = solver.sample(2000, 0, 0, 2009);
    EXPECT_NEAR(twice.radiation_energy / equator.radiation_energy, 0.25, 1e-12);
}

TEST(Antenna3D,MaskIsExplicitAndSnapshotsAreFiniteOutsideSource) {
    const Antenna3D solver({.side = 32});
    EXPECT_FALSE(solver.sample(0, 0, 0, 10).valid);
    EXPECT_TRUE(std::isnan(solver.sample(0, 0, 0, 10).energy));
    const auto snapshot = solver.snapshot(16);
    std::size_t masked = 0;
    for (std::size_t i = 0; i < snapshot[12].size(); ++i) {
        if (!snapshot[12][i]) ++masked;
        for (std::size_t field = 0; field < 12; ++field) {
            if (snapshot[12][i]) EXPECT_TRUE(std::isfinite(snapshot[field][i]));
            else EXPECT_TRUE(std::isnan(snapshot[field][i]));
        }
        if (snapshot[12][i]) EXPECT_GE(snapshot[10][i], 0);
    }
    EXPECT_GT(masked, 0);
}

TEST(Antenna3D,RejectsInvalidParametersBeforeSnapshotAllocation) {
    EXPECT_THROW((void)Antenna3D({.side = std::numeric_limits<std::size_t>::max()}), std::invalid_argument);
    EXPECT_THROW((void)Antenna3D({.dt = 1}), std::invalid_argument);
    EXPECT_THROW((void)Antenna3D({.period = 0}), std::invalid_argument);
    EXPECT_THROW((void)Antenna3D({.moment = std::numeric_limits<double>::infinity()}), std::invalid_argument);
    EXPECT_THROW((void)Antenna3D({.exclusion_radius = 0}), std::invalid_argument);
}
