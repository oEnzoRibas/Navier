#pragma once
#include "antenna3d.hpp"

namespace navier::examples {
struct InterferenceSample {
    AntennaSample combined;
    double incoherent_energy = 0, interference_energy = 0, radiation_interference = 0;
};

// Two prescribed, parallel Z dipoles centered at (+/- separation/2,0,0).
// Superpose E/B first, THEN form energy and Poynting: summing energies alone
// would discard interference. Each source has its own retarded distance.
class Interference3D {
    Antenna3D first_, second_;
    double separation_;
public:
    static constexpr std::size_t field_count = 16;
    using Snapshot = std::array<std::vector<float>, field_count>;
    explicit Interference3D(AntennaOptions options = {}, double separation = 4,
                           double phase = std::numbers::pi / 2)
        : first_(options), second_([&] { options.phase += phase; return options; }()), separation_(separation) {
        if (!std::isfinite(separation) || separation < 0 ||
            separation / 2 + options.exclusion_radius >= options.half_extent)
            throw std::invalid_argument("Both source masks must fit inside the observation window");
    }
    [[nodiscard]] const Antenna3D& first() const { return first_; }
    [[nodiscard]] const Antenna3D& second() const { return second_; }
    [[nodiscard]] double separation() const { return separation_; }
    [[nodiscard]] InterferenceSample sample(double x, double y, double z, double time) const {
        const auto a = first_.sample(x + separation_ / 2, y, z, time);
        const auto b = second_.sample(x - separation_ / 2, y, z, time);
        InterferenceSample result;
        auto& c = result.combined;
        if (!a.valid || !b.valid) {
            c = !a.valid ? a : b;
            result.incoherent_energy = result.interference_energy = result.radiation_interference =
                std::numeric_limits<double>::quiet_NaN();
            return result;
        }
        c.potential = a.potential + b.potential;
        for (std::size_t i = 0; i < 3; ++i) {
            c.electric[i] = a.electric[i] + b.electric[i];
            c.magnetic[i] = a.magnetic[i] + b.magnetic[i];
            c.vector_potential[i] = a.vector_potential[i] + b.vector_potential[i];
            c.radiation_electric[i] = a.radiation_electric[i] + b.radiation_electric[i];
            c.radiation_magnetic[i] = a.radiation_magnetic[i] + b.radiation_magnetic[i];
            c.energy += 0.5 * (c.electric[i] * c.electric[i] + c.magnetic[i] * c.magnetic[i]);
            c.radiation_energy += 0.5 * (c.radiation_electric[i] * c.radiation_electric[i] +
                                         c.radiation_magnetic[i] * c.radiation_magnetic[i]);
            result.interference_energy += a.electric[i] * b.electric[i] + a.magnetic[i] * b.magnetic[i];
            result.radiation_interference += a.radiation_electric[i] * b.radiation_electric[i] +
                                               a.radiation_magnetic[i] * b.radiation_magnetic[i];
        }
        c.poynting = {c.electric[1] * c.magnetic[2] - c.electric[2] * c.magnetic[1],
                      c.electric[2] * c.magnetic[0] - c.electric[0] * c.magnetic[2],
                      c.electric[0] * c.magnetic[1] - c.electric[1] * c.magnetic[0]};
        result.incoherent_energy = a.energy + b.energy;
        return result;
    }
    [[nodiscard]] Snapshot snapshot(double time) const {
        const auto n = first_.options().side;
        Snapshot fields;
        for (auto& f : fields) f.resize(n * n * n);
        for (std::size_t z = 0; z < n; ++z)
            for (std::size_t y = 0; y < n; ++y)
                for (std::size_t x = 0; x < n; ++x) {
                    const auto at = (z * n + y) * n + x;
                    const auto s = sample(first_.coordinate(x), first_.coordinate(y), first_.coordinate(z), time);
                    for (std::size_t i = 0; i < 3; ++i) {
                        fields[i][at] = static_cast<float>(s.combined.electric[i]);
                        fields[3 + i][at] = static_cast<float>(s.combined.magnetic[i]);
                        fields[6 + i][at] = static_cast<float>(s.combined.poynting[i]);
                    }
                    fields[9][at] = static_cast<float>(s.combined.potential);
                    fields[10][at] = static_cast<float>(s.combined.energy);
                    fields[11][at] = static_cast<float>(s.combined.radiation_energy);
                    fields[12][at] = s.combined.valid ? 1.F : 0.F;
                    fields[13][at] = static_cast<float>(s.interference_energy);
                    fields[14][at] = static_cast<float>(s.incoherent_energy);
                    fields[15][at] = static_cast<float>(s.radiation_interference);
                }
        return fields;
    }
};
} // namespace navier::examples
