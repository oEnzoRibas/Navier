#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace navier::examples {

// 2D FDTD solver for Maxwell's equations in TMz polarization.
//
// Fields:
//   Ez(x, y)
//   Hx(x, y)
//   Hy(x, y)
//
// Normalized units:
//   epsilon = 1
//   mu      = 1
//   c       = 1
//   dx = dy = 1
//
// Perfect Electric Conductor (PEC) boundaries are imposed by
// forcing Ez = 0 along the domain boundary.
class Electromagnetic2D {
public:
    explicit Electromagnetic2D(
        std::size_t side,
        double dt = 0.5
    )
        : n_(side),
          dt_(dt),
          ez_(n_ * n_, 0.0),
          hx_(n_ * n_, 0.0),
          hy_(n_ * n_, 0.0) {

        if (side < 16 || side > 4096) {
            throw std::invalid_argument(
                "Electromagnetic grid must be in [16, 4096]"
            );
        }

        // For dx = dy = c = 1:
        //
        // dt <= 1 / sqrt(2)
        //
        if (!std::isfinite(dt_) ||
            dt_ <= 0.0 ||
            dt_ > 1.0 / std::sqrt(2.0)) {

            throw std::invalid_argument(
                "FDTD timestep violates the 2D CFL condition"
            );
        }
    }

    void advance() {
        update_magnetic_field();
        update_electric_field();
        inject_source();
        apply_pec_boundary();

        ++step_;
    }

    [[nodiscard]]
    std::vector<double> values() const {
        const auto cells = n_ * n_;

        std::vector<double> result;
        result.reserve(3 * cells);

        // Planar layout:
        //
        // [ Ez ][ Hx ][ Hy ]

        result.insert(
            result.end(),
            ez_.begin(),
            ez_.end()
        );

        result.insert(
            result.end(),
            hx_.begin(),
            hx_.end()
        );

        result.insert(
            result.end(),
            hy_.begin(),
            hy_.end()
        );

        return result;
    }

    [[nodiscard]]
    double electromagnetic_energy() const {
        double energy = 0.0;

        for (std::size_t i = 0; i < ez_.size(); ++i) {
            energy += 0.5 * (
                ez_[i] * ez_[i] +
                hx_[i] * hx_[i] +
                hy_[i] * hy_[i]
            );
        }

        return energy;
    }

    [[nodiscard]]
    double max_electric_field() const {
        double maximum = 0.0;

        for (const auto value : ez_) {
            maximum =
                std::max(maximum, std::abs(value));
        }

        return maximum;
    }

    [[nodiscard]]
    double dt() const noexcept {
        return dt_;
    }

    [[nodiscard]]
    double courant_number() const noexcept {
        // c = dx = 1
        return dt_;
    }

private:
    [[nodiscard]]
    std::size_t index(
        std::size_t x,
        std::size_t y
    ) const noexcept {
        return y * n_ + x;
    }

    void update_magnetic_field() {
        // Hx^{n+1/2}
        for (std::size_t y = 0; y + 1 < n_; ++y) {
            for (std::size_t x = 0; x < n_; ++x) {
                const auto i = index(x, y);

                hx_[i] -= dt_ * (
                    ez_[index(x, y + 1)] -
                    ez_[i]
                );
            }
        }

        // Hy^{n+1/2}
        for (std::size_t y = 0; y < n_; ++y) {
            for (std::size_t x = 0; x + 1 < n_; ++x) {
                const auto i = index(x, y);

                hy_[i] += dt_ * (
                    ez_[index(x + 1, y)] -
                    ez_[i]
                );
            }
        }
    }

    void update_electric_field() {
        for (std::size_t y = 1; y + 1 < n_; ++y) {
            for (std::size_t x = 1; x + 1 < n_; ++x) {
                const auto i = index(x, y);

                const double d_hy_dx =
                    hy_[i] -
                    hy_[index(x - 1, y)];

                const double d_hx_dy =
                    hx_[i] -
                    hx_[index(x, y - 1)];

                ez_[i] +=
                    dt_ * (d_hy_dx - d_hx_dy);
            }
        }
    }

    void inject_source() {
        // Gaussian pulse at the center of the domain.
        //
        // The source acts only during the initial portion
        // of the simulation and then decays naturally.

        constexpr double center_time = 30.0;
        constexpr double width = 10.0;

        const double t =
            (static_cast<double>(step_) - center_time)
            / width;

        const double pulse =
            std::exp(-(t * t));

        const auto center = n_ / 2;

        // Soft source.
        ez_[index(center, center)] += pulse;
    }

    void apply_pec_boundary() {
        // Perfect Electric Conductor:
        //
        // Ez = 0 on all walls.

        for (std::size_t x = 0; x < n_; ++x) {
            ez_[index(x, 0)] = 0.0;
            ez_[index(x, n_ - 1)] = 0.0;
        }

        for (std::size_t y = 0; y < n_; ++y) {
            ez_[index(0, y)] = 0.0;
            ez_[index(n_ - 1, y)] = 0.0;
        }
    }

    std::size_t n_;
    double dt_;
    std::uint64_t step_ = 0;

    std::vector<double> ez_;
    std::vector<double> hx_;
    std::vector<double> hy_;
};

} // namespace navier::examples