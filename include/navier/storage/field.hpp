#pragma once
#include <navier/format/binary.hpp>
#include <cmath>
#include <span>
#include <string>
#include <variant>
#include <vector>
namespace navier::storage {
enum class ScalarType : std::uint8_t { float32=1, float64=2 };
enum class CheckpointKind : std::uint32_t { analysis=0, restart=1 };
struct FieldDescriptor {
    std::string name;
    ScalarType scalar_type=ScalarType::float64;
    std::vector<std::uint64_t> dimensions;
    std::string unit;
    bool operator==(const FieldDescriptor&)const=default;
    [[nodiscard]] std::uint64_t count()const{
        if(name.empty()||name.size()>255||unit.size()>255||dimensions.empty()||dimensions.size()>8)throw std::invalid_argument("Invalid field name, unit or rank");
        std::uint64_t count=1;for(const auto n:dimensions){if(!n)throw std::invalid_argument("Zero field dimension");count=format::checked_mul(count,n);}return count;
    }
    [[nodiscard]] std::uint64_t byte_size()const{
        if(scalar_type!=ScalarType::float32&&scalar_type!=ScalarType::float64)throw std::invalid_argument("Unsupported field scalar type");
        return format::checked_mul(count(),scalar_type==ScalarType::float32?4U:8U);
    }
};
struct FieldView {FieldDescriptor descriptor;std::variant<std::span<const float>,std::span<const double>> values;};
struct Field {FieldDescriptor descriptor;std::variant<std::vector<float>,std::vector<double>> values;};
struct Metric {std::string name;double value;std::string unit;bool operator==(const Metric&)const=default;};
struct CheckpointDescriptor {
    std::uint64_t step;double time;std::uint64_t generation;
    CheckpointKind kind=CheckpointKind::analysis;
    std::vector<FieldDescriptor> fields;std::vector<Metric> metrics;
};
struct ScientificCheckpoint {CheckpointDescriptor descriptor;std::vector<Field> fields;};
} // namespace navier::storage
