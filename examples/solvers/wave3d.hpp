#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace navier::examples {

/*
 * 3D transient scalar wave equation:
 *
 *     u_tt = c^2 ∇²u
 *
 * Explicit second-order finite differences in space and time:
 *
 *     u_next = 2 u_curr - u_prev + (c dt)^2 Laplacian(u_curr)
 *
 * Boundary condition:
 *     u = 0 on the six faces (Dirichlet / reflective with sign inversion)
 *
 * Exported fields:
 *   - amplitude
 *   - energy_density
 */
class Wave3D {
public:
    explicit Wave3D(
        std::size_t side,
        double c = 1.0,
        double dt = 0.45
    )
        : n_(side),
          c_(c),
          dt_(dt),
          prev_(n_ * n_ * n_, 0.0),
          curr_(n_ * n_ * n_, 0.0),
          next_(n_ * n_ * n_, 0.0) {

        if (side < 16 || side > 128) {
            throw std::invalid_argument(
                "Wave3D grid must be in [16, 128]"
            );
        }

        if (!std::isfinite(c_) || c_ <= 0.0) {
            throw std::invalid_argument(
                "Wave speed must be positive"
            );
        }

        if (!std::isfinite(dt_) || dt_ <= 0.0) {
            throw std::invalid_argument(
                "Timestep must be positive"
            );
        }

        /*
         * CFL in 3D with dx = dy = dz = 1:
         *
         * c * dt <= 1 / sqrt(3)
         */
        if (c_ * dt_ > 1.0 / std::sqrt(3.0)) {
            throw std::invalid_argument(
                "Wave3D timestep violates the 3D CFL condition"
            );
        }
    }

    void advance() {
        const double lambda2 = (c_ * dt_) * (c_ * dt_);

        // Interior update.
        for (std::size_t z = 1; z + 1 < n_; ++z) {
            for (std::size_t y = 1; y + 1 < n_; ++y) {
                for (std::size_t x = 1; x + 1 < n_; ++x) {
                    const auto i = index(x, y, z);

                    const double laplacian =
                        curr_[index(x + 1, y, z)] +
                        curr_[index(x - 1, y, z)] +
                        curr_[index(x, y + 1, z)] +
                        curr_[index(x, y - 1, z)] +
                        curr_[index(x, y, z + 1)] +
                        curr_[index(x, y, z - 1)] -
                        6.0 * curr_[i];

                    next_[i] =
                        2.0 * curr_[i]
                        - prev_[i]
                        + lambda2 * laplacian;
                }
            }
        }

        inject_source();
        apply_dirichlet_boundary();

        prev_.swap(curr_);
        curr_.swap(next_);

        std::fill(next_.begin(), next_.end(), 0.0);

        ++step_;
    }

    [[nodiscard]]
    std::vector<double> values() const {
        const auto cells = curr_.size();
        std::vector<double> result(2 * cells);

        // amplitude
        for (std::size_t i = 0; i < cells; ++i) {
            result[i] = curr_[i];
        }

        // energy_density
        for (std::size_t z = 0; z < n_; ++z) {
            for (std::size_t y = 0; y < n_; ++y) {
                for (std::size_t x = 0; x < n_; ++x) {
                    const auto i = index(x, y, z);

                    const double ut =
                        (curr_[i] - prev_[i]) / dt_;

                    const double ux =
                        0.5 * (
                            sample_curr(
                                static_cast<int>(x) + 1,
                                static_cast<int>(y),
                                static_cast<int>(z)
                            )
                            -
                            sample_curr(
                                static_cast<int>(x) - 1,
                                static_cast<int>(y),
                                static_cast<int>(z)
                            )
                        );

                    const double uy =
                        0.5 * (
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y) + 1,
                                static_cast<int>(z)
                            )
                            -
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y) - 1,
                                static_cast<int>(z)
                            )
                        );

                    const double uz =
                        0.5 * (
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y),
                                static_cast<int>(z) + 1
                            )
                            -
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y),
                                static_cast<int>(z) - 1
                            )
                        );

                    result[cells + i] =
                        0.5 * (
                            ut * ut +
                            c_ * c_ * (
                                ux * ux +
                                uy * uy +
                                uz * uz
                            )
                        );
                }
            }
        }

        return result;
    }

    [[nodiscard]]
    double total_energy() const {
        double energy = 0.0;

        for (std::size_t z = 0; z < n_; ++z) {
            for (std::size_t y = 0; y < n_; ++y) {
                for (std::size_t x = 0; x < n_; ++x) {
                    const auto i = index(x, y, z);

                    const double ut =
                        (curr_[i] - prev_[i]) / dt_;

                    const double ux =
                        0.5 * (
                            sample_curr(
                                static_cast<int>(x) + 1,
                                static_cast<int>(y),
                                static_cast<int>(z)
                            )
                            -
                            sample_curr(
                                static_cast<int>(x) - 1,
                                static_cast<int>(y),
                                static_cast<int>(z)
                            )
                        );

                    const double uy =
                        0.5 * (
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y) + 1,
                                static_cast<int>(z)
                            )
                            -
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y) - 1,
                                static_cast<int>(z)
                            )
                        );

                    const double uz =
                        0.5 * (
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y),
                                static_cast<int>(z) + 1
                            )
                            -
                            sample_curr(
                                static_cast<int>(x),
                                static_cast<int>(y),
                                static_cast<int>(z) - 1
                            )
                        );

                    energy +=
                        0.5 * (
                            ut * ut +
                            c_ * c_ * (
                                ux * ux +
                                uy * uy +
                                uz * uz
                            )
                        );
                }
            }
        }

        return energy;
    }

    [[nodiscard]]
    double max_amplitude() const {
        double maximum = 0.0;

        for (const auto value : curr_) {
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
    double wave_speed() const noexcept {
        return c_;
    }

    [[nodiscard]]
    double courant_number() const noexcept {
        return c_ * dt_;
    }

private:
    [[nodiscard]]
    std::size_t index(
        std::size_t x,
        std::size_t y,
        std::size_t z
    ) const noexcept {
        return (z * n_ + y) * n_ + x;
    }

    [[nodiscard]]
    double sample_curr(
        int x,
        int y,
        int z
    ) const noexcept {
        if (x < 0 || y < 0 || z < 0 ||
            x >= static_cast<int>(n_) ||
            y >= static_cast<int>(n_) ||
            z >= static_cast<int>(n_)) {
            return 0.0;
        }

        return curr_[
            index(
                static_cast<std::size_t>(x),
                static_cast<std::size_t>(y),
                static_cast<std::size_t>(z)
            )
        ];
    }

    void inject_source() {
        /*
         * Transient Gaussian-modulated sinusoidal pulse.
         *
         * It acts near the beginning of the simulation
         * and naturally leaves the domain afterwards.
         */

        const double t =
            static_cast<double>(step_);

        const double envelope =
            std::exp(
                -std::pow((t - 16.0) / 6.0, 2.0)
            );

        const double carrier =
            std::sin(0.55 * t);

        const double pulse =
            envelope * carrier;

        const auto cx = n_ / 2;
        const auto cy = n_ / 2;
        const auto cz = n_ / 2;

        next_[index(cx, cy, cz)] += pulse;
    }

    void apply_dirichlet_boundary() {
        for (std::size_t y = 0; y < n_; ++y) {
            for (std::size_t x = 0; x < n_; ++x) {
                next_[index(x, y, 0)] = 0.0;
                next_[index(x, y, n_ - 1)] = 0.0;
            }
        }

        for (std::size_t z = 0; z < n_; ++z) {
            for (std::size_t x = 0; x < n_; ++x) {
                next_[index(x, 0, z)] = 0.0;
                next_[index(x, n_ - 1, z)] = 0.0;
            }
        }

        for (std::size_t z = 0; z < n_; ++z) {
            for (std::size_t y = 0; y < n_; ++y) {
                next_[index(0, y, z)] = 0.0;
                next_[index(n_ - 1, y, z)] = 0.0;
            }
        }
    }

    std::size_t n_;
    double c_;
    double dt_;
    std::uint64_t step_ = 0;

    std::vector<double> prev_;
    std::vector<double> curr_;
    std::vector<double> next_;
};

} // namespace navier::examples