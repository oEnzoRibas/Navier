#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace navier::examples {
inline void check_grid(std::size_t side) {
    if (side < 8 || side > 512) throw std::invalid_argument("Grid side must be in [8, 512]");
}

// Explicit five-point heat equation on [0,1]^2, zero Dirichlet boundary.
class Heat2D {
public:
    explicit Heat2D(std::size_t side, double diffusivity = 0.1)
        : side_(side), diffusivity_(diffusivity) {
        check_grid(side);
        if (!std::isfinite(diffusivity) || diffusivity <= 0)
            throw std::invalid_argument("Diffusivity must be finite and positive");
        values_.resize(side * side);
        scratch_.resize(side * side);
        const double dx = 1.0 / static_cast<double>(side - 1);
        dt_ = 0.2 * dx * dx / diffusivity;
        for (std::size_t y = 1; y + 1 < side; ++y)
            for (std::size_t x = 1; x + 1 < side; ++x)
                values_[y * side + x] = std::sin(std::numbers::pi * static_cast<double>(x) * dx)
                                      * std::sin(std::numbers::pi * static_cast<double>(y) * dx);
    }
    void advance() {
        // alpha*dt/dx^2 = 0.2 <= 1/4: a convex stencil preserves the maximum principle.
        for (std::size_t y = 1; y + 1 < side_; ++y)
            for (std::size_t x = 1; x + 1 < side_; ++x) {
                const auto i = y * side_ + x;
                scratch_[i] = values_[i] + 0.2 * (values_[i - 1] + values_[i + 1]
                    + values_[i - side_] + values_[i + side_] - 4 * values_[i]);
            }
        values_.swap(scratch_);
    }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }
    [[nodiscard]] double dt() const noexcept { return dt_; }
    [[nodiscard]] double diffusivity() const noexcept { return diffusivity_; }
private:
    std::size_t side_;
    double diffusivity_;
    double dt_;
    std::vector<double> values_;
    std::vector<double> scratch_;
};

// BGK D2Q9 in lattice units (dx=dt=1). Periodic Taylor-Green vortex,
// low Mach velocity. This solver belongs to the examples, never to the index.
enum class FlowPattern { TaylorGreen, ShearLayer };
class Fluid2D {
    static constexpr std::array<int, 9> cx{0, 1, 0, -1, 0, 1, -1, -1, 1};
    static constexpr std::array<int, 9> cy{0, 0, 1, 0, -1, 1, 1, -1, -1};
    static constexpr std::array<double, 9> weights{4.0/9, 1.0/9, 1.0/9, 1.0/9, 1.0/9,
                                                 1.0/36, 1.0/36, 1.0/36, 1.0/36};
    struct Moments { double rho = 0; double ux = 0; double uy = 0; };
    static Moments moments(const std::array<double, 9>& f) {
        Moments result;
        for (std::size_t q = 0; q < 9; ++q) {
            result.rho += f[q];
            result.ux += f[q] * cx[q];
            result.uy += f[q] * cy[q];
        }
        if (!std::isfinite(result.rho) || result.rho <= 0) throw std::runtime_error("Invalid fluid density");
        result.ux /= result.rho;
        result.uy /= result.rho;
        return result;
    }
    static double equilibrium(std::size_t q, double rho, double ux, double uy) {
        const double cu = cx[q] * ux + cy[q] * uy;
        return weights[q] * rho * (1 + 3 * cu + 4.5 * cu * cu - 1.5 * (ux * ux + uy * uy));
    }
public:
    explicit Fluid2D(std::size_t side, double viscosity = 0.02, double speed = 0.03, FlowPattern pattern = FlowPattern::TaylorGreen)
        : side_(side) {
        check_grid(side);
        if (!std::isfinite(viscosity) || viscosity <= 0 ||
            !std::isfinite(speed) || speed < 0 || speed > 0.1)
            throw std::invalid_argument("Invalid viscosity or low-Mach initial speed");
        omega_ = 1.0 / (0.5 + 3 * viscosity);
        populations_.resize(side * side);
        scratch_.resize(side * side);
        const double k = 2 * std::numbers::pi / static_cast<double>(side);
        for (std::size_t y = 0; y < side; ++y)
            for (std::size_t x = 0; x < side; ++x) {
                const double xx = k * static_cast<double>(x);
                const double yy = k * static_cast<double>(y);
                double ux = speed * std::sin(xx) * std::cos(yy);
                double uy = -speed * std::cos(xx) * std::sin(yy);
                                double rho = 1 + 0.75 * speed * speed * (std::cos(2 * xx) + std::cos(2 * yy));
                if (pattern == FlowPattern::ShearLayer) {
                    const double fraction = static_cast<double>(y) / static_cast<double>(side);
                    ux = speed * std::tanh(40 * (fraction < 0.5 ? fraction - 0.25 : 0.75 - fraction));
                    uy = speed * 0.075 * (std::sin(4*xx) + 0.5*std::sin(2*xx + 0.3) + 0.25*std::sin(5*xx + 1.2));
                    rho = 1;
                }
                for (std::size_t q = 0; q < 9; ++q)
                    populations_[y * side + x][q] = equilibrium(q, rho, ux, uy);
            }
    }
    void advance() {
        const auto n = static_cast<int>(side_);
        for (std::size_t y = 0; y < side_; ++y)
            for (std::size_t x = 0; x < side_; ++x) {
                const auto i = y * side_ + x;
                const auto m = moments(populations_[i]);
                for (std::size_t q = 0; q < 9; ++q) {
                    const auto tx = static_cast<std::size_t>((static_cast<int>(x) + cx[q] + n) % n);
                    const auto ty = static_cast<std::size_t>((static_cast<int>(y) + cy[q] + n) % n);
                    scratch_[ty * side_ + tx][q] = populations_[i][q]
                        - omega_ * (populations_[i][q] - equilibrium(q, m.rho, m.ux, m.uy));
                }
            }
        populations_.swap(scratch_);
    }
    // Three row-major planes: density, x velocity, y velocity.
    [[nodiscard]] std::vector<double> values() const {
        const auto cells = side_ * side_;
        std::vector<double> result(3 * cells);
        for (std::size_t i = 0; i < cells; ++i) {
            const auto m = moments(populations_[i]);
            result[i] = m.rho;
            result[cells + i] = m.ux;
            result[2 * cells + i] = m.uy;
        }
        return result;
    }
private:
    std::size_t side_;
    double omega_;
    std::vector<std::array<double, 9>> populations_;
    std::vector<std::array<double, 9>> scratch_;
};
} // namespace navier::examples
