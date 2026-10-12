#pragma once
#include "simulation2d.hpp"
namespace navier::examples {
// Periodic D3Q19 BGK solver in lattice units. ABC initial velocity is a
// divergence-free, fully three-dimensional Beltrami field.
class Fluid3D {
    static constexpr std::array<int,19> cx{0,1,-1,0,0,0,0,1,-1,1,-1,1,-1,1,-1,0,0,0,0};
    static constexpr std::array<int,19> cy{0,0,0,1,-1,0,0,1,-1,-1,1,0,0,0,0,1,-1,1,-1};
    static constexpr std::array<int,19> cz{0,0,0,0,0,1,-1,0,0,0,0,1,-1,-1,1,1,-1,-1,1};
    static double weight(std::size_t q) { return q==0 ? 1.0/3 : q<7 ? 1.0/18 : 1.0/36; }
    struct Moments { double rho=0,u=0,v=0,w=0; };
    static Moments moments(const std::array<double,19>& f) {
        Moments m;
        for(std::size_t q=0;q<19;++q) { m.rho+=f[q];m.u+=f[q]*cx[q];m.v+=f[q]*cy[q];m.w+=f[q]*cz[q]; }
        if(!std::isfinite(m.rho)||m.rho<=0) throw std::runtime_error("Invalid 3D fluid density");
        m.u/=m.rho;m.v/=m.rho;m.w/=m.rho;return m;
    }
    static double equilibrium(std::size_t q,const Moments& m) {
        const double cu=cx[q]*m.u+cy[q]*m.v+cz[q]*m.w;
        return weight(q)*m.rho*(1+3*cu+4.5*cu*cu-1.5*(m.u*m.u+m.v*m.v+m.w*m.w));
    }
public:
    explicit Fluid3D(std::size_t side,double viscosity=0.02,double amplitude=0.035):n_(side) {
        if(side<8||side>64||!std::isfinite(viscosity)||viscosity<=0||!std::isfinite(amplitude)||amplitude<0||amplitude>0.04)
            throw std::invalid_argument("3D grid must be 8..64; viscosity positive; ABC amplitude 0..0.04");
        omega_=1/(0.5+3*viscosity);f_.resize(n_*n_*n_);scratch_.resize(f_.size());
        const double k=2*std::numbers::pi/static_cast<double>(n_);
        for(std::size_t z=0;z<n_;++z)for(std::size_t y=0;y<n_;++y)for(std::size_t x=0;x<n_;++x) {
            const double xx=k*static_cast<double>(x),yy=k*static_cast<double>(y),zz=k*static_cast<double>(z);
            Moments m;
            m.u=amplitude*(std::sin(zz)+std::cos(yy));
            m.v=amplitude*(std::sin(xx)+std::cos(zz));
            m.w=amplitude*(std::sin(yy)+std::cos(xx));
            m.rho=1-1.5*(m.u*m.u+m.v*m.v+m.w*m.w-3*amplitude*amplitude);
            for(std::size_t q=0;q<19;++q)f_[(z*n_+y)*n_+x][q]=equilibrium(q,m);
        }
    }
    void advance() {
        const int side=static_cast<int>(n_);
        for(std::size_t z=0;z<n_;++z)for(std::size_t y=0;y<n_;++y)for(std::size_t x=0;x<n_;++x) {
            const auto i=(z*n_+y)*n_+x;const auto m=moments(f_[i]);
            for(std::size_t q=0;q<19;++q) {
                const auto tx=static_cast<std::size_t>((static_cast<int>(x)+cx[q]+side)%side);
                const auto ty=static_cast<std::size_t>((static_cast<int>(y)+cy[q]+side)%side);
                const auto tz=static_cast<std::size_t>((static_cast<int>(z)+cz[q]+side)%side);
                scratch_[(tz*n_+ty)*n_+tx][q]=f_[i][q]-omega_*(f_[i][q]-equilibrium(q,m));
            }
        }
        f_.swap(scratch_);
    }
    [[nodiscard]] std::vector<double> values() const {
        const auto cells=f_.size();std::vector<double> result(4*cells);
        for(std::size_t i=0;i<cells;++i) { const auto m=moments(f_[i]);result[i]=m.rho;result[cells+i]=m.u;result[2*cells+i]=m.v;result[3*cells+i]=m.w; }
        return result;
    }
private:
    std::size_t n_;double omega_;std::vector<std::array<double,19>> f_,scratch_;
};
} // namespace navier::examples
