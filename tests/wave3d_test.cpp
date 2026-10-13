#include "../examples/solvers/wave3d.hpp"
#include <gtest/gtest.h>
#include <limits>
TEST(Wave3D,ValidatesSideBeforeAllocationAndEnforcesCFL){
EXPECT_THROW((void)navier::examples::Wave3D(std::numeric_limits<std::size_t>::max()),std::invalid_argument);
EXPECT_THROW((void)navier::examples::Wave3D(32,1,1),std::invalid_argument);
EXPECT_THROW((void)navier::examples::Wave3D(32,0,.45),std::invalid_argument);
EXPECT_THROW((void)navier::examples::Wave3D(32,1,.45,std::numeric_limits<double>::quiet_NaN()),std::invalid_argument);
}
TEST(Wave3D,ZeroStateWithDisabledSourceRemainsZero){navier::examples::Wave3D solver(16,1,.45,0);for(int i=0;i<50;++i)solver.advance();EXPECT_EQ(solver.max_amplitude(),0);EXPECT_EQ(solver.total_energy(),0);for(const auto v:solver.values())EXPECT_EQ(v,0);}
TEST(Wave3D,FiniteDeterministicAxisSymmetryAndDirichletFaces){constexpr std::size_t n=16,cells=n*n*n;navier::examples::Wave3D first(n),second(n);for(int i=0;i<80;++i){first.advance();second.advance();}const auto values=first.values();EXPECT_EQ(values,second.values());EXPECT_GT(first.total_energy(),0);EXPECT_TRUE(std::isfinite(first.total_energy()));for(std::size_t z=0;z<n;++z)for(std::size_t y=0;y<n;++y)for(std::size_t x=0;x<n;++x){const auto at=(z*n+y)*n+x,transposed=(z*n+x)*n+y;EXPECT_TRUE(std::isfinite(values[at]));EXPECT_GE(values[cells+at],0);EXPECT_NEAR(values[at],values[transposed],1e-12);if(x==0||y==0||z==0||x==n-1||y==n-1||z==n-1)EXPECT_EQ(values[at],0);}}
