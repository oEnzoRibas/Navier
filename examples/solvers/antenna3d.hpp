#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace navier::examples {

// Ideal, prescribed electric dipole along Z in free space. This evaluates the
// retarded Maxwell solution, not a finite-wire/FDTD or conductor simulation.
// Reference: Feynman Lectures II, 21-4, equations 21.23, 21.25--21.27.
struct AntennaOptions {
    std::size_t side = 48;
    double half_extent = 8.0;
    double dt = 0.05;
    double period = 4.0;
    double turn_on = 4.0;
    double moment = 1.0;
    double exclusion_radius = 0.6;
};

struct DipoleMoment { double value, first, second; };
struct AntennaSample {
    std::array<double, 3> electric{}, magnetic{}, poynting{}, vector_potential{};
    double potential = 0, energy = 0, radiation_energy = 0;
    bool valid = true;
};

class Antenna3D {
    AntennaOptions options_;
    double omega_;
public:
    static constexpr std::size_t field_count = 13;
    using Snapshot = std::array<std::vector<float>, field_count>;

    explicit Antenna3D(AntennaOptions options = {}) : options_(options), omega_(0) {
        if (options.side < 16 || options.side > 96)
            throw std::invalid_argument("Antenna grid side must be in [16, 96]");
        for (const auto value : {options.half_extent, options.dt, options.period,
                                options.turn_on, options.exclusion_radius})
            if (!std::isfinite(value) || value <= 0)
                throw std::invalid_argument("Antenna lengths and times must be positive and finite");
        if (!std::isfinite(options.moment) || std::abs(options.moment) > 100)
            throw std::invalid_argument("Dipole moment must be finite and have magnitude <= 100");
        if (options.half_extent < 1 || options.half_extent > 100 ||
            options.period < 0.5 || options.period > 100 || options.turn_on < options.dt ||
            options.turn_on > 100 || options.exclusion_radius < 0.05 ||
            options.exclusion_radius >= options.half_extent / 2)
            throw std::invalid_argument("Unsupported antenna domain, period, ramp or exclusion radius");
        if (options.dt > options.period / 40 || spacing() > options.period / 6)
            throw std::invalid_argument("Use at least 40 time samples and 6 grid samples per wavelength");
        omega_ = 2 * std::numbers::pi / options.period;
    }

    [[nodiscard]] const AntennaOptions& options() const { return options_; }
    [[nodiscard]] double spacing() const {
        return 2 * options_.half_extent / static_cast<double>(options_.side - 1);
    }
    [[nodiscard]] double coordinate(std::size_t index) const {
        return -options_.half_extent + static_cast<double>(index) * spacing();
    }

    // C2 quintic ramp: value, first and second derivatives join continuously.
    [[nodiscard]] DipoleMoment source(double time) const {
        if (!std::isfinite(time)) throw std::invalid_argument("Time must be finite");
        if (time <= 0) return {0, 0, 0};
        double envelope = 1, first = 0, second = 0;
        if (time < options_.turn_on) {
            const auto s = time / options_.turn_on;
            envelope = s * s * s * (10 + s * (-15 + 6 * s));
            first = 30 * s * s * (s - 1) * (s - 1) / options_.turn_on;
            second = 60 * s * (2 * s * s - 3 * s + 1) /
                     (options_.turn_on * options_.turn_on);
        }
        const auto sine = std::sin(omega_ * time), cosine = std::cos(omega_ * time);
        return {options_.moment * envelope * sine,
                options_.moment * (first * sine + envelope * omega_ * cosine),
                options_.moment * (second * sine + 2 * first * omega_ * cosine -
                                   envelope * omega_ * omega_ * sine)};
    }

    [[nodiscard]] AntennaSample sample(double x, double y, double z, double time) const {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(time))
            throw std::invalid_argument("Coordinates and time must be finite");
        const auto r = std::hypot(x, y, z);
        AntennaSample out;
        if (r < options_.exclusion_radius) {
            // The singular point source is outside the represented physics.
            const auto invalid = std::numeric_limits<double>::quiet_NaN();
            out.electric.fill(invalid); out.magnetic.fill(invalid); out.poynting.fill(invalid);
            out.vector_potential.fill(invalid);
            out.potential = out.energy = out.radiation_energy = invalid;
            out.valid = false;
            return out;
        }
        // Normalized units: c=epsilon=mu=1. The source has been zero for t<=0.
        const auto p = source(time - r);
        const std::array<double, 3> n{x / r, y / r, z / r};
        constexpr auto k = 1 / (4 * std::numbers::pi);
        const auto near = k * (p.value / (r * r * r) + p.first / (r * r));
        const auto far = k * p.second / r;
        const auto magnetic = k * (p.first / (r * r) + p.second / r);
        std::array<double, 3> radiation{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto axial = axis == 2 ? 1.0 : 0.0;
            radiation[axis] = far * (n[axis] * n[2] - axial);
            out.electric[axis] = near * (3 * n[axis] * n[2] - axial) + radiation[axis];
        }
        out.magnetic = {-magnetic * n[1], magnetic * n[0], 0};
        out.poynting = {out.electric[1] * out.magnetic[2] - out.electric[2] * out.magnetic[1],
                        out.electric[2] * out.magnetic[0] - out.electric[0] * out.magnetic[2],
                        out.electric[0] * out.magnetic[1] - out.electric[1] * out.magnetic[0]};
        out.potential = k * n[2] * (p.value / (r * r) + p.first / r);
        out.vector_potential[2] = k * p.first / r;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            out.energy += 0.5 * (out.electric[axis] * out.electric[axis] +
                                 out.magnetic[axis] * out.magnetic[axis]);
            out.radiation_energy += radiation[axis] * radiation[axis];
        }
        return out;
    }

    // X fastest. Float32 snapshots sample double-precision analytical fields.
    // Fields: E[3], B[3], S[3], phi, full energy, radiation-only energy, mask.
    [[nodiscard]] Snapshot snapshot(double time) const {
        const auto n = options_.side;
        Snapshot result;
        for (auto& field : result) field.resize(n * n * n);
        for (std::size_t z = 0; z < n; ++z)
            for (std::size_t y = 0; y < n; ++y)
                for (std::size_t x = 0; x < n; ++x) {
                    const auto at = (z * n + y) * n + x;
                    const auto value = sample(coordinate(x), coordinate(y), coordinate(z), time);
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        result[axis][at] = static_cast<float>(value.electric[axis]);
                        result[3 + axis][at] = static_cast<float>(value.magnetic[axis]);
                        result[6 + axis][at] = static_cast<float>(value.poynting[axis]);
                    }
                    result[9][at] = static_cast<float>(value.potential);
                    result[10][at] = static_cast<float>(value.energy);
                    result[11][at] = static_cast<float>(value.radiation_energy);
                    result[12][at] = value.valid ? 1.F : 0.F;
                }
        return result;
    }
};
} // namespace navier::examples
