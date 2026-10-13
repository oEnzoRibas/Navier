#include "../examples/solvers/interference3d.hpp"
#include <gtest/gtest.h>
using navier::examples::Interference3D;

TEST(Interference3D,CoherentSourcesGiveFourTimesEnergyAndOppositePhasesCancel) {
    const Interference3D coherent({},0,0), opposite({},0,std::numbers::pi);
    const auto one=coherent.first().sample(3,2,1,9.3);
    const auto both=coherent.sample(3,2,1,9.3);
    EXPECT_NEAR(both.combined.energy,4*one.energy,1e-14);
    EXPECT_NEAR(both.interference_energy,2*one.energy,1e-14);
    const auto zero=opposite.sample(3,2,1,9.3);
    EXPECT_NEAR(zero.combined.energy,0,1e-28);
}
TEST(Interference3D,EnergyIdentityIncludesCrossTermsAndPoyntingUsesTotalFields) {
    const Interference3D solver;
    bool constructive=false,destructive=false;
    for (int i=-6;i<=6;++i) {
        const auto s=solver.sample(static_cast<double>(i),3,1,18.2);
        EXPECT_NEAR(s.combined.energy,s.incoherent_energy+s.interference_energy,1e-14);
        EXPECT_GE(s.combined.energy,0);
        EXPECT_NEAR(s.combined.poynting[0],s.combined.electric[1]*s.combined.magnetic[2]-s.combined.electric[2]*s.combined.magnetic[1],1e-14);
        constructive=constructive||s.interference_energy>0;destructive=destructive||s.interference_energy<0;
    }
    EXPECT_TRUE(constructive);EXPECT_TRUE(destructive);
}
TEST(Interference3D,EachSourceHasItsOwnDelayAndMask) {
    const Interference3D solver;
    EXPECT_FALSE(solver.sample(-2,0,0,12).combined.valid);
    EXPECT_FALSE(solver.sample(2,0,0,12).combined.valid);
    EXPECT_EQ(solver.sample(0,6,0,1).combined.energy,0);
    const auto s=solver.sample(-3,0,0,2.5);
    const auto a=solver.first().sample(-1,0,0,2.5);
    for (std::size_t i=0;i<3;++i) EXPECT_NEAR(s.combined.electric[i],a.electric[i],1e-14);
}
TEST(Interference3D,TotalFieldSatisfiesVacuumMaxwellEquationsAwayFromBothSources) {
    const Interference3D solver;
    constexpr double h=1e-4,t=9.3;
    const std::array<double,3> point{0.3,2.4,1.1};
    std::array<std::array<double,3>,3> de{},db{};
    for (std::size_t i=0;i<3;++i) {
        auto p=point,m=point;p[i]+=h;m[i]-=h;
        const auto a=solver.sample(p[0],p[1],p[2],t).combined,b=solver.sample(m[0],m[1],m[2],t).combined;
        for (std::size_t j=0;j<3;++j) { de[i][j]=(a.electric[j]-b.electric[j])/(2*h);db[i][j]=(a.magnetic[j]-b.magnetic[j])/(2*h); }
    }
    const auto a=solver.sample(point[0],point[1],point[2],t+h).combined,b=solver.sample(point[0],point[1],point[2],t-h).combined;
    EXPECT_NEAR(de[0][0]+de[1][1]+de[2][2],0,1e-9);
    EXPECT_NEAR(db[0][0]+db[1][1]+db[2][2],0,1e-9);
    for (std::size_t i=0;i<3;++i) {const auto j=(i+1)%3,k=(i+2)%3;
        EXPECT_NEAR(de[j][k]-de[k][j],-(a.magnetic[i]-b.magnetic[i])/(2*h),1e-9);
        EXPECT_NEAR(db[j][k]-db[k][j],(a.electric[i]-b.electric[i])/(2*h),1e-9);
    }
}
TEST(Interference3D,RejectsInvalidSourcePlacementAndPhase) {
    EXPECT_THROW((void)Interference3D({},20),std::invalid_argument);
    EXPECT_THROW((void)Interference3D({},-1),std::invalid_argument);
    EXPECT_THROW((void)Interference3D({},4,std::numeric_limits<double>::infinity()),std::invalid_argument);
}
