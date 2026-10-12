#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace navier::examples {

// 3D electromagnetic pulse in a PEC cavity.
// Discrete FDTD-style update in normalized units:
//
// epsilon = 1
// mu      = 1
// c       = 1
// dx = dy = dz = 1
//
// For stability in 3D:
// dt <= 1 / sqrt(3)
//
// Exported fields:
//   energy_density
//   velocity_x   -> actually Poynting_x
//   velocity_y   -> actually Poynting_y
//   velocity_z   -> actually Poynting_z
//   electric_magnitude
//
// The "velocity_*" naming is intentional so the current Navier 3D
// viewer can render streamlines and arrows without changes.
class Electromagnetic3D {
public:
    explicit Electromagnetic3D(
        std::size_t side,
        double dt = 0.5
    )
        : n_(side),
          dt_(dt),
          ex_(n_ * n_ * n_, 0.0),
          ey_(n_ * n_ * n_, 0.0),
          ez_(n_ * n_ * n_, 0.0),
          hx_(n_ * n_ * n_, 0.0),
          hy_(n_ * n_ * n_, 0.0),
          hz_(n_ * n_ * n_, 0.0) {

        if (side < 12 || side > 96) {
            throw std::invalid_argument(
                "Electromagnetic 3D grid must be in [12, 96]"
            );
        }

        if (!std::isfinite(dt_) ||
            dt_ <= 0.0 ||
            dt_ > 1.0 / std::sqrt(3.0)) {
            throw std::invalid_argument(
                "3D timestep violates the CFL condition"
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
        const std::size_t cells = n_ * n_ * n_;

        std::vector<double> result(5 * cells);

        for (std::size_t i = 0; i < cells; ++i) {
            const double e2 =
                ex_[i] * ex_[i] +
                ey_[i] * ey_[i] +
                ez_[i] * ez_[i];

            const double h2 =
                hx_[i] * hx_[i] +
                hy_[i] * hy_[i] +
                hz_[i] * hz_[i];

            const double sx =
                ey_[i] * hz_[i] -
                ez_[i] * hy_[i];

            const double sy =
                ez_[i] * hx_[i] -
                ex_[i] * hz_[i];

            const double sz =
                ex_[i] * hy_[i] -
                ey_[i] * hx_[i];

            result[i] = 0.5 * (e2 + h2);                  // energy_density
            result[cells + i] = sx;                       // velocity_x  ~ Poynting_x
            result[2 * cells + i] = sy;                   // velocity_y  ~ Poynting_y
            result[3 * cells + i] = sz;                   // velocity_z  ~ Poynting_z
            result[4 * cells + i] = std::sqrt(e2);       // electric_magnitude
        }

        return result;
    }

    [[nodiscard]]
    double total_energy() const {
        double energy = 0.0;

        for (std::size_t i = 0; i < ex_.size(); ++i) {
            energy += 0.5 * (
                ex_[i] * ex_[i] +
                ey_[i] * ey_[i] +
                ez_[i] * ez_[i] +
                hx_[i] * hx_[i] +
                hy_[i] * hy_[i] +
                hz_[i] * hz_[i]
            );
        }

        return energy;
    }

    [[nodiscard]]
    double max_electric_field() const {
        double maximum = 0.0;

        for (std::size_t i = 0; i < ex_.size(); ++i) {
            const double mag = std::sqrt(
                ex_[i] * ex_[i] +
                ey_[i] * ey_[i] +
                ez_[i] * ez_[i]
            );

            maximum = std::max(maximum, mag);
        }

        return maximum;
    }

    [[nodiscard]]
    double dt() const noexcept {
        return dt_;
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

    void update_magnetic_field() {
        for (std::size_t z = 0; z + 1 < n_; ++z) {
            for (std::size_t y = 0; y + 1 < n_; ++y) {
                for (std::size_t x = 0; x + 1 < n_; ++x) {
                    const auto i = index(x, y, z);

                    const double dezy =
                        ez_[index(x, y + 1, z)] - ez_[i];
                    const double deyz =
                        ey_[index(x, y, z + 1)] - ey_[i];

                    const double dexz =
                        ex_[index(x, y, z + 1)] - ex_[i];
                    const double dezx =
                        ez_[index(x + 1, y, z)] - ez_[i];

                    const double deyx =
                        ey_[index(x + 1, y, z)] - ey_[i];
                    const double dexy =
                        ex_[index(x, y + 1, z)] - ex_[i];

                    hx_[i] -= dt_ * (dezy - deyz);
                    hy_[i] -= dt_ * (dexz - dezx);
                    hz_[i] -= dt_ * (deyx - dexy);
                }
            }
        }
    }

    void update_electric_field() {
        for (std::size_t z = 1; z + 1 < n_; ++z) {
            for (std::size_t y = 1; y + 1 < n_; ++y) {
                for (std::size_t x = 1; x + 1 < n_; ++x) {
                    const auto i = index(x, y, z);

                    const double dhzdy =
                        hz_[i] - hz_[index(x, y - 1, z)];
                    const double dhydz =
                        hy_[i] - hy_[index(x, y, z - 1)];

                    const double dhxdz =
                        hx_[i] - hx_[index(x, y, z - 1)];
                    const double dhzdx =
                        hz_[i] - hz_[index(x - 1, y, z)];

                    const double dhydx =
                        hy_[i] - hy_[index(x - 1, y, z)];
                    const double dhxdy =
                        hx_[i] - hx_[index(x, y - 1, z)];

                    ex_[i] += dt_ * (dhzdy - dhydz);
                    ey_[i] += dt_ * (dhxdz - dhzdx);
                    ez_[i] += dt_ * (dhydx - dhxdy);
                }
            }
        }
    }

    void inject_source() {
        // Gaussian-modulated sinusoidal pulse near the center.
        const std::size_t cx = n_ / 2;
        const std::size_t cy = n_ / 2;
        const std::size_t cz = n_ / 2;

        const double t =
            static_cast<double>(step_);

        const double envelope =
            std::exp(-std::pow((t - 18.0) / 7.0, 2.0));

        const double carrier =
            std::sin(0.42 * t);

        const double pulse =
            envelope * carrier;

        const auto c = index(cx, cy, cz);

        ez_[c] += pulse;
        ex_[c] += 0.35 * pulse;
        ey_[c] -= 0.25 * pulse;
    }

    void apply_pec_boundary() {
        // Simple PEC-style boundary condition:
        // zero electric field on the six faces.

        for (std::size_t y = 0; y < n_; ++y) {
            for (std::size_t x = 0; x < n_; ++x) {
                ex_[index(x, y, 0)] = 0.0;
                ey_[index(x, y, 0)] = 0.0;
                ez_[index(x, y, 0)] = 0.0;

                ex_[index(x, y, n_ - 1)] = 0.0;
                ey_[index(x, y, n_ - 1)] = 0.0;
                ez_[index(x, y, n_ - 1)] = 0.0;
            }
        }

        for (std::size_t z = 0; z < n_; ++z) {
            for (std::size_t x = 0; x < n_; ++x) {
                ex_[index(x, 0, z)] = 0.0;
                ey_[index(x, 0, z)] = 0.0;
                ez_[index(x, 0, z)] = 0.0;

                ex_[index(x, n_ - 1, z)] = 0.0;
                ey_[index(x, n_ - 1, z)] = 0.0;
                ez_[index(x, n_ - 1, z)] = 0.0;
            }
        }

        for (std::size_t z = 0; z < n_; ++z) {
            for (std::size_t y = 0; y < n_; ++y) {
                ex_[index(0, y, z)] = 0.0;
                ey_[index(0, y, z)] = 0.0;
                ez_[index(0, y, z)] = 0.0;

                ex_[index(n_ - 1, y, z)] = 0.0;
                ey_[index(n_ - 1, y, z)] = 0.0;
                ez_[index(n_ - 1, y, z)] = 0.0;
            }
        }
    }

    std::size_t n_;
    double dt_;
    std::uint64_t step_ = 0;

    std::vector<double> ex_;
    std::vector<double> ey_;
    std::vector<double> ez_;
    std::vector<double> hx_;
    std::vector<double> hy_;
    std::vector<double> hz_;
};

} // namespace navier::examples