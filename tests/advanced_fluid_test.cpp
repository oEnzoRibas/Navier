#include "../examples/solvers/fluid3d.hpp"
#include <gtest/gtest.h>
#include <numeric>
namespace {
TEST(Shear2D, MassAndFiniteFlow) {
    navier::examples::Fluid2D solver(48,0.02,0.08,navier::examples::FlowPattern::ShearLayer);
    const auto initial=solver.values();constexpr std::size_t cells=48*48;
    for(int step=0;step<500;++step)solver.advance();
    const auto values=solver.values();
    EXPECT_NEAR(std::accumulate(values.begin(),values.begin()+cells,0.0),static_cast<double>(cells),1e-8);
    bool changed=false;
    for(std::size_t i=0;i<values.size();++i) { ASSERT_TRUE(std::isfinite(values[i]));if(std::abs(values[i]-initial[i])>1e-3)changed=true; }
    EXPECT_TRUE(changed);
}
TEST(Fluid3D, RestEquilibriumAndValidation) {
    navier::examples::Fluid3D solver(8,0.02,0);
    for(int i=0;i<20;++i)solver.advance();
    const auto values=solver.values();
    for(std::size_t i=0;i<512;++i)EXPECT_NEAR(values[i],1,1e-13);
    for(std::size_t i=512;i<values.size();++i)EXPECT_NEAR(values[i],0,1e-13);
    EXPECT_THROW(navier::examples::Fluid3D too_large(65),std::invalid_argument);
}
TEST(Fluid3D, MassMomentumAndViscousDecay) {
    navier::examples::Fluid3D solver(16);constexpr std::size_t cells=16*16*16;
    const auto initial=solver.values();
    for(int i=0;i<200;++i)solver.advance();
    const auto values=solver.values();
    EXPECT_NEAR(std::accumulate(values.begin(),values.begin()+cells,0.0),static_cast<double>(cells),1e-8);
    double initial_energy=0,final_energy=0;
    for(std::size_t component=1;component<=3;++component) {
        double momentum=0;
        for(std::size_t i=0;i<cells;++i) {
            ASSERT_TRUE(std::isfinite(values[component*cells+i]));
            momentum+=values[i]*values[component*cells+i];
            initial_energy+=initial[i]*initial[component*cells+i]*initial[component*cells+i];
            final_energy+=values[i]*values[component*cells+i]*values[component*cells+i];
        }
        EXPECT_NEAR(momentum,0,1e-9);
    }
    EXPECT_GT(final_energy,0);EXPECT_LT(final_energy,initial_energy);
}
} // namespace
