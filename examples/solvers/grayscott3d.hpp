#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace navier::examples {
// Gray–Scott reaction diffusion, periodic unit lattice, forward Euler / 7-point Laplacian.
class GrayScott3D {
    std::array<std::size_t,3> n_;double dt_,du_,dv_,feed_,kill_;std::vector<double> u_,v_,next_u_,next_v_;
    std::size_t index(std::size_t x,std::size_t y,std::size_t z)const{return (z*n_[1]+y)*n_[0]+x;}
    double laplacian(const std::vector<double>& field,std::size_t x,std::size_t y,std::size_t z)const{return field[index((x+1)%n_[0],y,z)]+field[index((x+n_[0]-1)%n_[0],y,z)]+field[index(x,(y+1)%n_[1],z)]+field[index(x,(y+n_[1]-1)%n_[1],z)]+field[index(x,y,(z+1)%n_[2])]+field[index(x,y,(z+n_[2]-1)%n_[2])]-6*field[index(x,y,z)];}
public:
    explicit GrayScott3D(std::array<std::size_t,3> dimensions={32,32,32},double dt=.25,double du=.16,double dv=.08,double feed=.04,double kill=.06,bool seed=true):n_(dimensions),dt_(dt),du_(du),dv_(dv),feed_(feed),kill_(kill){
        for(const auto n:n_)if(n<8||n>128)throw std::invalid_argument("GrayScott dimensions must each be 8..128");
        if(!std::isfinite(dt)||dt<=0||dt>.25||!std::isfinite(du)||du<=0||!std::isfinite(dv)||dv<=0||!std::isfinite(feed)||feed<0||feed>1||!std::isfinite(kill)||kill<0||kill>1||6*std::max(du,dv)*dt>1)throw std::invalid_argument("Invalid GrayScott parameters or diffusion timestep");
        const auto cells=n_[0]*n_[1]*n_[2];u_.assign(cells,1);v_.assign(cells,0);next_u_.resize(cells);next_v_.resize(cells);if(!seed)return;
        const double radius=static_cast<double>(*std::min_element(n_.begin(),n_.end()))/6;
        for(std::size_t z=0;z<n_[2];++z)for(std::size_t y=0;y<n_[1];++y)for(std::size_t x=0;x<n_[0];++x){const double dx=static_cast<double>(x)-static_cast<double>(n_[0])/2,dy=static_cast<double>(y)-static_cast<double>(n_[1])/2,dz=static_cast<double>(z)-static_cast<double>(n_[2])/2;if(dx*dx+dy*dy+dz*dz<radius*radius){const auto at=index(x,y,z);const double noise=(static_cast<double>((at*1664525U+1013904223U)&65535U)/65535-.5)*.005;u_[at]=.5+noise;v_[at]=.25+noise;}}
    }
    void advance(){for(std::size_t z=0;z<n_[2];++z)for(std::size_t y=0;y<n_[1];++y)for(std::size_t x=0;x<n_[0];++x){const auto at=index(x,y,z);const auto reaction=u_[at]*v_[at]*v_[at];next_u_[at]=u_[at]+dt_*(du_*laplacian(u_,x,y,z)-reaction+feed_*(1-u_[at]));next_v_[at]=v_[at]+dt_*(dv_*laplacian(v_,x,y,z)+reaction-(feed_+kill_)*v_[at]);if(!std::isfinite(next_u_[at])||!std::isfinite(next_v_[at])||next_u_[at]<0||next_v_[at]<0)throw std::runtime_error("Invalid GrayScott concentration: reduce timestep");}u_.swap(next_u_);v_.swap(next_v_);}
    [[nodiscard]] const std::vector<double>& species_u()const{return u_;}
    [[nodiscard]] const std::vector<double>& species_v()const{return v_;}
    [[nodiscard]] double dt()const{return dt_;}
};
}
