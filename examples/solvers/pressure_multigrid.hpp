#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace navier::examples {
// Geometric multigrid for the cell-centered Neumann pressure Poisson problem.
// All levels cover [0,1]^2. Restriction averages 2x2 cell residuals; prolongation
// interpolates between cell centers, with mirrored Neumann boundary values.
class PressureMultigrid {
    struct Level {
        explicit Level(std::size_t side):n(side),h2(1.0/static_cast<double>(side*side)),p(side*side),b(side*side),r(side*side) {}
        std::size_t n;double h2;std::vector<double> p,b,r;
    };
public:
    explicit PressureMultigrid(std::size_t side) {
        if(side<8||side>1024)throw std::invalid_argument("Cavity grid side must be in [8, 1024]");
        levels_.emplace_back(side);
        while(side>8&&side%2==0){side/=2;levels_.emplace_back(side);}
    }
    void solve(const std::vector<double>& rhs,std::vector<double>& pressure,double tolerance) {
        auto& fine=levels_.front();fine.p=pressure;
        for(std::size_t i=0;i<rhs.size();++i)fine.b[i]=-rhs[i];
        remove_mean(fine.b);
        bool converged=false;
        for(int cycle=0;cycle<150;++cycle){
            vcycle(0);
            if(residual(fine)<tolerance){converged=true;break;}
        }
        if(!converged)throw std::runtime_error("Multigrid pressure solve failed to converge");
        remove_mean(fine.p);pressure=fine.p;
    }
private:
    static void remove_mean(std::vector<double>& v) {
        const double mean=std::accumulate(v.begin(),v.end(),0.0)/static_cast<double>(v.size());
        for(auto& x:v)x-=mean;
    }
    static void smooth(Level& l,int sweeps) {
        for(int sweep=0;sweep<sweeps;++sweep)for(std::size_t color=0;color<2;++color)
            for(std::size_t y=0;y<l.n;++y)for(std::size_t x=(y+color)%2;x<l.n;x+=2){
                const auto i=y*l.n+x;double sum=0,degree=0;
                if(x>0){sum+=l.p[i-1];++degree;}if(x+1<l.n){sum+=l.p[i+1];++degree;}
                if(y>0){sum+=l.p[i-l.n];++degree;}if(y+1<l.n){sum+=l.p[i+l.n];++degree;}
                l.p[i]=(sum+l.h2*l.b[i])/degree;
            }
    }
    static double residual(Level& l){
        double max=0;
        for(std::size_t y=0;y<l.n;++y)for(std::size_t x=0;x<l.n;++x){
            const auto i=y*l.n+x;double lap=0;
            if(x>0)lap+=l.p[i]-l.p[i-1];if(x+1<l.n)lap+=l.p[i]-l.p[i+1];
            if(y>0)lap+=l.p[i]-l.p[i-l.n];if(y+1<l.n)lap+=l.p[i]-l.p[i+l.n];
            l.r[i]=l.b[i]-lap/l.h2;max=std::max(max,std::abs(l.r[i]));
        }return max;
    }
    void vcycle(std::size_t level){
        auto& fine=levels_[level];
        if(level+1==levels_.size()){
            // Small coarsest solve. Odd grids also have a valid single-level
            // solve; more sweeps are needed when they cannot be coarsened.
            smooth(fine,static_cast<int>(std::max(std::size_t{200},fine.n*4)));
            remove_mean(fine.p);return;
        }
        smooth(fine,3);(void)residual(fine);
        auto& coarse=levels_[level+1];std::fill(coarse.p.begin(),coarse.p.end(),0);
        for(std::size_t y=0;y<coarse.n;++y)for(std::size_t x=0;x<coarse.n;++x){
            const auto i=(2*y)*fine.n+2*x;
            coarse.b[y*coarse.n+x]=.25*(fine.r[i]+fine.r[i+1]+fine.r[i+fine.n]+fine.r[i+fine.n+1]);
        }
        remove_mean(coarse.b);vcycle(level+1);
        const auto value=[&coarse](int x,int y){
            x=std::clamp(x,0,static_cast<int>(coarse.n)-1);y=std::clamp(y,0,static_cast<int>(coarse.n)-1);
            return coarse.p[static_cast<std::size_t>(y)*coarse.n+static_cast<std::size_t>(x)];
        };
        for(std::size_t y=0;y<fine.n;++y)for(std::size_t x=0;x<fine.n;++x){
            const double xx=(static_cast<double>(x)-.5)/2,yy=(static_cast<double>(y)-.5)/2;
            const int ix=static_cast<int>(std::floor(xx)),iy=static_cast<int>(std::floor(yy));
            const double a=xx-ix,b=yy-iy;
            fine.p[y*fine.n+x]+=(1-a)*(1-b)*value(ix,iy)+a*(1-b)*value(ix+1,iy)+(1-a)*b*value(ix,iy+1)+a*b*value(ix+1,iy+1);
        }
        smooth(fine,3);remove_mean(fine.p);
    }
    std::vector<Level> levels_;
};
} // namespace navier::examples
