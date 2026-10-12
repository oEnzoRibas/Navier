#pragma once
#include "simulation2d.hpp"
#include "pressure_multigrid.hpp"
#include <limits>
#include <numeric>

namespace navier::examples {
// Incompressible Navier-Stokes on a staggered MAC grid. u and v live on
// faces; pressure lives at cell centers. A moving top wall drives the cavity.
class Cavity2D {
public:
    explicit Cavity2D(std::size_t side, double reynolds = 100, double lid_speed = 1)
        : n_(side), lid_(lid_speed), pressure_solver_(side) {
        if(side<8||side>1024) throw std::invalid_argument("Cavity grid side must be in [8, 1024]");
        if (!std::isfinite(reynolds) || reynolds <= 0 ||
            !std::isfinite(lid_speed) || lid_speed < 0 || lid_speed > 1)
            throw std::invalid_argument("Invalid Reynolds number or lid speed");
        h_ = 1.0 / static_cast<double>(side);
        viscosity_ = 1.0 / reynolds;
        dt_ = std::min(0.2 * h_ / std::max(lid_speed, 1.0), 0.2 * h_ * h_ / viscosity_);
        u_.resize((n_ + 1) * n_);
        v_.resize(n_ * (n_ + 1));
        us_ = u_;
        vs_ = v_;
        p_.resize(n_ * n_);
        rhs_.resize(n_ * n_);
    }
    void advance() {
        const double inv_h = 1 / h_;
        const double inv_h2 = inv_h * inv_h;
        // First-order upwind convection and centered diffusion. Ghost values
        // impose tangential no-slip and the moving lid without pressure coupling.
        for (std::size_t j = 0; j < n_; ++j) {
            for (std::size_t i = 1; i < n_; ++i) {
                const double u = u_[ui(i,j)];
                const double v = 0.25 * (v_[vi(i-1,j)] + v_[vi(i,j)] + v_[vi(i-1,j+1)] + v_[vi(i,j+1)]);
                const double left = u_[ui(i-1,j)], right = u_[ui(i+1,j)];
                const double bottom = j == 0 ? -u : u_[ui(i,j-1)];
                const double top = j + 1 == n_ ? 2 * lid_ - u : u_[ui(i,j+1)];
                const double du_dx = (u >= 0 ? u - left : right - u) * inv_h;
                const double du_dy = (v >= 0 ? u - bottom : top - u) * inv_h;
                us_[ui(i,j)] = u + dt_ * (-u * du_dx - v * du_dy
                    + viscosity_ * (left + right + top + bottom - 4 * u) * inv_h2);
            }
        }
        for (std::size_t j = 1; j < n_; ++j) {
            for (std::size_t i = 0; i < n_; ++i) {
                const double v = v_[vi(i,j)];
                const double u = 0.25 * (u_[ui(i,j-1)] + u_[ui(i+1,j-1)] + u_[ui(i,j)] + u_[ui(i+1,j)]);
                const double left = i == 0 ? -v : v_[vi(i-1,j)];
                const double right = i + 1 == n_ ? -v : v_[vi(i+1,j)];
                const double bottom = v_[vi(i,j-1)], top = v_[vi(i,j+1)];
                const double dv_dx = (u >= 0 ? v - left : right - v) * inv_h;
                const double dv_dy = (v >= 0 ? v - bottom : top - v) * inv_h;
                vs_[vi(i,j)] = v + dt_ * (-u * dv_dx - v * dv_dy
                    + viscosity_ * (left + right + top + bottom - 4 * v) * inv_h2);
            }
        }
        double mean_rhs = 0;
        for (std::size_t j = 0; j < n_; ++j)
            for (std::size_t i = 0; i < n_; ++i) {
                const double div = (us_[ui(i+1,j)] - us_[ui(i,j)] + vs_[vi(i,j+1)] - vs_[vi(i,j)]) * inv_h;
                rhs_[j*n_+i] = div / dt_;
                mean_rhs += rhs_[j*n_+i];
            }
        // Compatibility for the Neumann Poisson problem; mean is roundoff only
        // because the predictor has exactly zero boundary-normal flux.
        mean_rhs /= static_cast<double>(n_ * n_);
        for (auto& r : rhs_) r -= mean_rhs;
        solve_pressure();
        for (std::size_t j = 0; j < n_; ++j)
            for (std::size_t i = 1; i < n_; ++i)
                u_[ui(i,j)] = us_[ui(i,j)] - dt_ * (p_[j*n_+i] - p_[j*n_+i-1]) * inv_h;
        for (std::size_t j = 1; j < n_; ++j)
            for (std::size_t i = 0; i < n_; ++i)
                v_[vi(i,j)] = vs_[vi(i,j)] - dt_ * (p_[j*n_+i] - p_[(j-1)*n_+i]) * inv_h;
        if (!std::isfinite(max_divergence()) || max_divergence() > 1e-4)
            throw std::runtime_error("Pressure projection did not converge; reduce grid or timestep");
    }
    [[nodiscard]] std::vector<double> values() const {
        const auto cells = n_ * n_;
        std::vector<double> result(3 * cells);
        for (std::size_t j = 0; j < n_; ++j)
            for (std::size_t i = 0; i < n_; ++i) {
                const auto c = j*n_+i;
                result[c] = 0.5 * (u_[ui(i,j)] + u_[ui(i+1,j)]);
                result[cells+c] = 0.5 * (v_[vi(i,j)] + v_[vi(i,j+1)]);
                result[2*cells+c] = p_[c];
            }
        return result;
    }
    [[nodiscard]] double max_divergence() const {
        double maximum = 0;
        for (std::size_t j = 0; j < n_; ++j)
            for (std::size_t i = 0; i < n_; ++i)
                maximum = std::max(maximum, std::abs((u_[ui(i+1,j)] - u_[ui(i,j)]
                    + v_[vi(i,j+1)] - v_[vi(i,j)]) / h_));
        return maximum;
    }
    [[nodiscard]] double boundary_normal_max() const {
        double maximum = 0;
        for (std::size_t i = 0; i < n_; ++i) {
            maximum = std::max({maximum, std::abs(u_[ui(0,i)]), std::abs(u_[ui(n_,i)]),
                               std::abs(v_[vi(i,0)]), std::abs(v_[vi(i,n_)])});
        }
        return maximum;
    }
    [[nodiscard]] double dt() const noexcept { return dt_; }
    [[nodiscard]] double viscosity() const noexcept { return viscosity_; }
private:
    [[nodiscard]] std::size_t ui(std::size_t i, std::size_t j) const { return j*(n_+1)+i; }
    [[nodiscard]] std::size_t vi(std::size_t i, std::size_t j) const { return j*n_+i; }
    void solve_pressure() {
        pressure_solver_.solve(rhs_,p_,1e-6/dt_);
    }
    std::size_t n_;
    double lid_, h_, viscosity_, dt_;
    std::vector<double> u_, v_, us_, vs_, p_, rhs_;
    PressureMultigrid pressure_solver_;
};
} // namespace navier::examples
