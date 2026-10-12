#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace navier::examples {

class Lorenz3D {
public:
    explicit Lorenz3D(
        std::size_t side,
        double dt = 0.005,
        double sigma = 10.0,
        double rho = 28.0,
        double beta = 8.0 / 3.0
    )
        : n_(side),
          dt_(dt),
          sigma_(sigma),
          rho_(rho),
          beta_(beta),
          density_(n_ * n_ * n_, 0.0) {

        if (side < 16 || side > 128) {
            throw std::invalid_argument(
                "Lorenz grid must be in [16, 128]"
            );
        }

        if (!std::isfinite(dt_) ||
            dt_ <= 0.0 ||
            dt_ > 0.02) {
            throw std::invalid_argument(
                "Lorenz timestep must be in (0, 0.02]"
            );
        }

        deposit();
    }

    void advance() {
        /*
         * RK4 integration.
         *
         * Euler works, but RK4 produces a substantially cleaner
         * attractor for visualization.
         */

        const auto k1 = derivative(x_, y_, z_);

        const auto k2 = derivative(
            x_ + 0.5 * dt_ * k1.x,
            y_ + 0.5 * dt_ * k1.y,
            z_ + 0.5 * dt_ * k1.z
        );

        const auto k3 = derivative(
            x_ + 0.5 * dt_ * k2.x,
            y_ + 0.5 * dt_ * k2.y,
            z_ + 0.5 * dt_ * k2.z
        );

        const auto k4 = derivative(
            x_ + dt_ * k3.x,
            y_ + dt_ * k3.y,
            z_ + dt_ * k3.z
        );

        x_ += dt_ / 6.0 *
            (k1.x + 2.0 * k2.x + 2.0 * k3.x + k4.x);

        y_ += dt_ / 6.0 *
            (k1.y + 2.0 * k2.y + 2.0 * k3.y + k4.y);

        z_ += dt_ / 6.0 *
            (k1.z + 2.0 * k2.z + 2.0 * k3.z + k4.z);

        if (!std::isfinite(x_) ||
            !std::isfinite(y_) ||
            !std::isfinite(z_)) {
            throw std::runtime_error(
                "Lorenz integration became non-finite"
            );
        }

        /*
         * Slowly fade older samples.
         *
         * This creates a luminous trail instead of permanently
         * filling every visited voxel.
         */
        if (step_ % 8 == 0) {
            for (auto& value : density_) {
                value *= 0.997;
            }
        }

        deposit();

        ++step_;
    }

    [[nodiscard]]
    std::vector<double> values() const {
        return density_;
    }

    [[nodiscard]]
    double x() const noexcept {
        return x_;
    }

    [[nodiscard]]
    double y() const noexcept {
        return y_;
    }

    [[nodiscard]]
    double z() const noexcept {
        return z_;
    }

    [[nodiscard]]
    double dt() const noexcept {
        return dt_;
    }

private:
    struct State {
        double x;
        double y;
        double z;
    };

    [[nodiscard]]
    State derivative(
        double x,
        double y,
        double z
    ) const noexcept {

        return {
            sigma_ * (y - x),

            x * (rho_ - z) - y,

            x * y - beta_ * z
        };
    }

    [[nodiscard]]
    std::size_t index(
        std::size_t x,
        std::size_t y,
        std::size_t z
    ) const noexcept {

        return (z * n_ + y) * n_ + x;
    }

    [[nodiscard]]
    std::size_t mapCoordinate(
        double value,
        double minimum,
        double maximum
    ) const {

        const double normalized =
            (value - minimum) /
            (maximum - minimum);

        const double clamped =
            std::clamp(
                normalized,
                0.0,
                1.0
            );

        return static_cast<std::size_t>(
            clamped *
            static_cast<double>(n_ - 1)
        );
    }

    void deposit() {
        /*
         * Approximate bounding box of the classical Lorenz attractor.
         *
         * x: [-30, 30]
         * y: [-30, 30]
         * z: [  0, 60]
         */

        const auto gx =
            mapCoordinate(x_, -30.0, 30.0);

        const auto gy =
            mapCoordinate(y_, -30.0, 30.0);

        const auto gz =
            mapCoordinate(z_, 0.0, 60.0);

        /*
         * Deposit into a small neighborhood to create a visible
         * volumetric trail instead of isolated single voxels.
         */

        constexpr int radius = 1;

        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {

                    const int px =
                        static_cast<int>(gx) + dx;

                    const int py =
                        static_cast<int>(gy) + dy;

                    const int pz =
                        static_cast<int>(gz) + dz;

                    if (px < 0 ||
                        py < 0 ||
                        pz < 0 ||
                        px >= static_cast<int>(n_) ||
                        py >= static_cast<int>(n_) ||
                        pz >= static_cast<int>(n_)) {
                        continue;
                    }

                    const double distance2 =
                        static_cast<double>(
                            dx * dx +
                            dy * dy +
                            dz * dz
                        );

                    const double weight =
                        std::exp(-0.8 * distance2);

                    auto& voxel =
                        density_[
                            index(
                                static_cast<std::size_t>(px),
                                static_cast<std::size_t>(py),
                                static_cast<std::size_t>(pz)
                            )
                        ];

                    voxel =
                        std::min(
                            1.0,
                            voxel + weight
                        );
                }
            }
        }
    }

    std::size_t n_;

    double dt_;

    double sigma_;
    double rho_;
    double beta_;

    /*
     * Initial condition slightly away from the origin.
     */
    double x_ = 0.1;
    double y_ = 0.0;
    double z_ = 0.0;

    std::uint64_t step_ = 0;

    std::vector<double> density_;
};

} // namespace navier::examples