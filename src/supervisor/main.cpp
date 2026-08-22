#include "rtsafe/protocol.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
    std::filesystem::path input;
    std::optional<std::filesystem::path> json_output;
    bool allow_invalid = false;
};

struct Report {
    std::uint64_t total_frames = 0U;
    std::uint64_t valid_frames = 0U;
    std::uint64_t invalid_frames = 0U;
    std::uint64_t sequence_gaps = 0U;
    std::uint64_t fault_frames = 0U;
    std::uint64_t degraded_frames = 0U;
    std::uint64_t safe_stop_frames = 0U;
    std::uint64_t last_timestamp_us = 0U;
    std::uint32_t last_sequence = 0U;
    bool has_sequence = false;
};

[[noreturn]] void usage_error(const std::string &message)
{
    throw std::runtime_error(
        message +
        "\nUsage: rtsafe-supervisor --input TELEMETRY.bin "
        "[--json REPORT.json] [--allow-invalid]"
    );
}

Options parse_options(int argc, char **argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto require_value = [&]() -> std::string {
            if (index + 1 >= argc) {
                usage_error("Missing value after " + argument);
            }
            return argv[++index];
        };

        if (argument == "--input") {
            options.input = require_value();
        } else if (argument == "--json") {
            options.json_output = require_value();
        } else if (argument == "--allow-invalid") {
            options.allow_invalid = true;
        } else if (argument == "--help" || argument == "-h") {
            usage_error("Telemetry validation and supervision tool");
        } else {
            usage_error("Unknown argument: " + argument);
        }
    }

    if (options.input.empty()) {
        usage_error("--input is required");
    }
    return options;
}

std::string to_json(const Report &report)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"total_frames\": " << report.total_frames << ",\n"
           << "  \"valid_frames\": " << report.valid_frames << ",\n"
           << "  \"invalid_frames\": " << report.invalid_frames << ",\n"
           << "  \"sequence_gaps\": " << report.sequence_gaps << ",\n"
           << "  \"fault_frames\": " << report.fault_frames << ",\n"
           << "  \"degraded_frames\": " << report.degraded_frames << ",\n"
           << "  \"safe_stop_frames\": " << report.safe_stop_frames << ",\n"
           << "  \"last_sequence\": " << report.last_sequence << ",\n"
           << "  \"last_timestamp_us\": " << report.last_timestamp_us << ",\n"
           << "  \"stream_valid\": "
           << (report.invalid_frames == 0U && report.sequence_gaps == 0U ? "true" : "false")
           << "\n}\n";
    return output.str();
}

Report inspect_stream(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open telemetry stream: " + path.string());
    }

    Report report;
    std::array<std::uint8_t, RTSAFE_TELEMETRY_FRAME_SIZE> frame{};
    while (input.read(
        reinterpret_cast<char *>(frame.data()),
        static_cast<std::streamsize>(frame.size())
    )) {
        ++report.total_frames;
        rtsafe_status_t status{};
        if (!rtsafe_decode_telemetry(frame.data(), frame.size(), &status)) {
            ++report.invalid_frames;
            continue;
        }

        ++report.valid_frames;
        if (report.has_sequence && status.sequence != report.last_sequence + 1U) {
            ++report.sequence_gaps;
        }
        report.has_sequence = true;
        report.last_sequence = status.sequence;
        report.last_timestamp_us = status.timestamp_us;
        if (status.fault_flags != RTSAFE_FAULT_NONE) {
            ++report.fault_frames;
        }
        if (status.state == RTSAFE_STATE_DEGRADED) {
            ++report.degraded_frames;
        }
        if (status.state == RTSAFE_STATE_SAFE_STOP) {
            ++report.safe_stop_frames;
        }
    }

    if (input.gcount() != 0) {
        ++report.invalid_frames;
        ++report.total_frames;
    }
    if (report.total_frames == 0U) {
        throw std::runtime_error("Telemetry stream is empty");
    }
    return report;
}

int run(const Options &options)
{
    const Report report = inspect_stream(options.input);
    const std::string json = to_json(report);

    if (options.json_output.has_value()) {
        const auto parent = options.json_output->parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::ofstream output(*options.json_output, std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "Cannot create report: " + options.json_output->string()
            );
        }
        output << json;
    } else {
        std::cout << json;
    }

    std::cout << "frames=" << report.total_frames
              << " valid=" << report.valid_frames
              << " invalid=" << report.invalid_frames
              << " gaps=" << report.sequence_gaps
              << " safe_stop=" << report.safe_stop_frames << '\n';

    const bool valid = report.invalid_frames == 0U && report.sequence_gaps == 0U;
    return valid || options.allow_invalid ? 0 : 2;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        return run(parse_options(argc, argv));
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

