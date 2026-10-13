#include "../examples/solvers/grayscott3d.hpp"
#include <gtest/gtest.h>
TEST(GrayScott3D,HomogeneousEquilibriumStaysExact){navier::examples::GrayScott3D solver({8,12,10},.25,.16,.08,.04,.06,false);for(int i=0;i<100;++i)solver.advance();for(auto u:solver.species_u())EXPECT_EQ(u,1);for(auto v:solver.species_v())EXPECT_EQ(v,0);}
TEST(GrayScott3D,FiniteDeterministicConcentrationsOnRectangularGrid){navier::examples::GrayScott3D first({12,16,8}),second({12,16,8});for(int i=0;i<500;++i){first.advance();second.advance();}EXPECT_EQ(first.species_u(),second.species_u());EXPECT_EQ(first.species_v(),second.species_v());for(auto u:first.species_u()){EXPECT_TRUE(std::isfinite(u));EXPECT_GE(u,0);EXPECT_LE(u,1.001);}for(auto v:first.species_v()){EXPECT_TRUE(std::isfinite(v));EXPECT_GE(v,0);EXPECT_LE(v,1.25);}}
TEST(GrayScott3D,RejectsInvalidGridAndTimestep){
EXPECT_THROW((void)navier::examples::GrayScott3D({0,8,8}),std::invalid_argument);
EXPECT_THROW((void)navier::examples::GrayScott3D({8,8,8},.5),std::invalid_argument);
EXPECT_THROW((void)navier::examples::GrayScott3D({8,8,8},.25,1,1),std::invalid_argument);
}
