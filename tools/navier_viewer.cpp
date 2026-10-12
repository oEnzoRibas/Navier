#include <navier/navier.hpp>
#include <navier/viewer_html.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
void json_string(std::ostream& out, std::string_view value) {
    out << '"';
    for (const char character : value) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32 || c == '<' || c == '>' || c == '&') {
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c)
                << std::dec << std::setfill(' ');
        } else out << static_cast<char>(c);
    }
    out << '"';
}
std::string read_metadata(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 1024*1024) throw std::runtime_error("Metadata file exceeds 1 MiB");
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read simulation metadata: " + path.string());
    std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // Escape HTML delimiters so arbitrary sidecar strings cannot end the JSON script element.
    std::string safe;
    for (const char c : result) {
        if (c == '<') safe += "\\u003c";
        else if (c == '>') safe += "\\u003e";
        else if (c == '&') safe += "\\u0026";
        else safe += c;
    }
    return safe;
}
void payload(std::ostream& out, const std::vector<double>& values) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::uint32_t accumulator = 0;
    unsigned bits = 0;
    out << '"';
    for (const double value : values) {
        if (!std::isfinite(value)) throw std::runtime_error("Viewer requires finite numerical values");
        const auto binary = std::bit_cast<std::uint64_t>(value);
        for (unsigned b = 0; b < 8; ++b) {
            accumulator = (accumulator << 8) | static_cast<std::uint32_t>((binary >> (8*b)) & 255U);
            bits += 8;
            while (bits >= 6) {
                bits -= 6;
                out << alphabet[(accumulator >> bits) & 63U];
            }
        }
    }
    if (bits != 0) out << alphabet[(accumulator << (6-bits)) & 63U];
    const auto bytes = values.size()*8;
    if (bytes % 3 == 1) out << "==";
    else if (bytes % 3 == 2) out << '=';
    out << '"';
}
void dataset(std::ostream& out, const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Checkpoint file does not exist: " + path.string());
    if (std::filesystem::file_size(path) > 256ULL*1024*1024) throw std::runtime_error("Checkpoint file exceeds viewer's 256 MiB limit");
    auto sidecar = path;
    sidecar += ".json";
    const auto metadata = read_metadata(sidecar);
    navier::storage::CheckpointStore store(path);
    if (store.size() == 0) throw std::runtime_error("No saved states in " + path.string());
    out << "{\"name\":";
    json_string(out, path.filename().string());
    out << ",\"metadata\":" << metadata << ",\"frames\":[";
    bool first = true;
    for (const auto& entry : store.range(0, std::numeric_limits<std::uint64_t>::max())) {
        const auto checkpoint = store.load(entry.step);
        if (!std::isfinite(entry.time)) throw std::runtime_error("Viewer requires finite checkpoint times");
        if (!first) out << ',';
        first = false;
        out << "{\"step\":\"" << entry.step << "\",\"time\":" << entry.time << ",\"data\":";
        payload(out, checkpoint.values);
        out << '}';
    }
    out << "]}";
    std::cout << "Added " << path.filename().string() << ": " << store.size() << " saved states\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
            std::cout << "Navier animated state viewer\nUsage: navier_viewer <output.html> [file.data ...]\n"
                      << "Without data files, creates a viewer with local file import.\n";
            return 0;
        }
        const std::filesystem::path output(argv[1]);
        if (output.extension() != ".html") throw std::invalid_argument("Viewer output must have the .html extension");
        for (int i = 2; i < argc; ++i) {
            const auto data = std::filesystem::weakly_canonical(argv[i]);
            if (std::filesystem::weakly_canonical(output) == data)
                throw std::invalid_argument("Output must differ from checkpoint inputs");
        }
        // Prepare fully in memory before opening the output: invalid input never
        // truncates an existing viewer. Input payloads are loaded via the B+ Tree.
        std::ostringstream serialized;
        serialized << std::setprecision(17) << '[';
        for (int i = 2; i < argc; ++i) {
            if (i > 2) serialized << ',';
            dataset(serialized, argv[i]);
        }
        serialized << ']';
        constexpr std::string_view marker = "__NAVIER_DATA__";
        std::string html;
        for (const auto part : navier::viewer::html_parts) html += part;
        const auto position = html.find(marker);
        if (position == std::string_view::npos) throw std::runtime_error("Viewer template marker is missing");
        std::ofstream file(output, std::ios::binary | std::ios::trunc);
        file << html.substr(0, position) << serialized.str()
             << html.substr(position + marker.size());
        file.close();
        if (!file) throw std::runtime_error("Cannot write viewer HTML");
        std::cout << "Viewer ready: " << std::filesystem::absolute(output).string()
                  << "\nOpen this HTML file in your browser to animate the saved states.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Navier viewer: " << error.what() << '\n';
        return 1;
    }
}
