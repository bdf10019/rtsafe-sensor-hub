#include "rtsafe/pipeline.h"
#include "rtsafe/protocol.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::string scenario = "nominal";
    std::uint32_t cycles = 2000U;
    std::uint32_t seed = 12345U;
    std::filesystem::path output_dir = "outputs/demo";
};

class DeterministicNoise {
public:
    explicit DeterministicNoise(std::uint32_t seed) : state_(seed) {}

    float symmetric(float amplitude)
    {
        state_ = state_ * 1664525U + 1013904223U;
        const auto sample = static_cast<float>((state_ >> 8U) & 0xFFFFU) / 65535.0F;
        return (sample * 2.0F - 1.0F) * amplitude;
    }

private:
    std::uint32_t state_;
};

[[noreturn]] void usage_error(const std::string &message)
{
    throw std::runtime_error(
        message +
        "\nUsage: rtsafe-simulator [--scenario nominal|imu-dropout|force-spike|"
        "heartbeat-loss|encoder-jump|deadline-overrun|crc-corruption|mixed] "
        "[--cycles N] [--seed N] [--output-dir PATH]"
    );
}

std::uint32_t parse_u32(const std::string &value, const std::string &name)
{
    std::size_t parsed = 0U;
    const auto result = std::stoull(value, &parsed, 10);
    if (parsed != value.size() || result > UINT32_MAX) {
        usage_error("Invalid value for " + name + ": " + value);
    }
    return static_cast<std::uint32_t>(result);
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

        if (argument == "--scenario") {
            options.scenario = require_value();
        } else if (argument == "--cycles") {
            options.cycles = parse_u32(require_value(), argument);
        } else if (argument == "--seed") {
            options.seed = parse_u32(require_value(), argument);
        } else if (argument == "--output-dir") {
            options.output_dir = require_value();
        } else if (argument == "--help" || argument == "-h") {
            usage_error("Deterministic real-time sensor and safety simulator");
        } else {
            usage_error("Unknown argument: " + argument);
        }
    }

    constexpr std::array<std::string_view, 8> scenarios = {
        "nominal",
        "imu-dropout",
        "force-spike",
        "heartbeat-loss",
        "encoder-jump",
        "deadline-overrun",
        "crc-corruption",
        "mixed"
    };
    bool scenario_valid = false;
    for (const auto candidate : scenarios) {
        scenario_valid = scenario_valid || options.scenario == candidate;
    }
    if (!scenario_valid) {
        usage_error("Unknown scenario: " + options.scenario);
    }
    if (options.cycles < 200U) {
        usage_error("--cycles must be at least 200");
    }
    return options;
}

float triangle_wave(std::uint32_t cycle, std::uint32_t period)
{
    const std::uint32_t phase = cycle % period;
    const float normalized = static_cast<float>(phase) / static_cast<float>(period);
    return normalized < 0.5F
        ? normalized * 4.0F - 1.0F
        : 3.0F - normalized * 4.0F;
}

void apply_scenario(
    const std::string &scenario,
    std::uint32_t cycle,
    rtsafe_input_t &input,
    std::uint32_t &execution_time_us,
    std::uint64_t &last_heartbeat_us
)
{
    const bool imu_dropout = scenario == "imu-dropout" || scenario == "mixed";
    const bool force_spike = scenario == "force-spike" || scenario == "mixed";

    if (imu_dropout && cycle >= 600U && cycle < 605U) {
        input.imu_valid = false;
    }
    if (force_spike && cycle >= 800U && cycle < 806U) {
        input.force_n = 180.0F;
    }
    if (scenario == "encoder-jump" && cycle == 700U) {
        input.encoder_ticks += 2000;
    }
    if (scenario == "deadline-overrun" && cycle >= 500U && cycle < 505U) {
        execution_time_us = 1200U;
    }
    if (scenario == "heartbeat-loss" && cycle >= 900U) {
        input.heartbeat_timestamp_us = last_heartbeat_us;
    }
}

void write_summary(
    const std::filesystem::path &path,
    const Options &options,
    const rtsafe_status_t &final_status,
    bool degraded_observed,
    bool safe_stop_observed,
    std::int64_t first_fault_cycle,
    std::int64_t first_safe_stop_cycle,
    std::uint32_t invalid_frames_injected
)
{
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create summary: " + path.string());
    }

    const std::int64_t reaction_cycles =
        first_fault_cycle >= 0 && first_safe_stop_cycle >= first_fault_cycle
        ? first_safe_stop_cycle - first_fault_cycle
        : -1;

    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"scenario\": \"" << options.scenario << "\",\n"
           << "  \"cycles\": " << options.cycles << ",\n"
           << "  \"seed\": " << options.seed << ",\n"
           << "  \"final_state\": \"" << rtsafe_state_name(final_status.state) << "\",\n"
           << "  \"degraded_observed\": " << std::boolalpha << degraded_observed << ",\n"
           << "  \"safe_stop_observed\": " << safe_stop_observed << ",\n"
           << "  \"first_fault_cycle\": " << first_fault_cycle << ",\n"
           << "  \"first_safe_stop_cycle\": " << first_safe_stop_cycle << ",\n"
           << "  \"fault_reaction_cycles\": " << reaction_cycles << ",\n"
           << "  \"deadline_misses\": " << final_status.deadline_misses << ",\n"
           << "  \"invalid_frames_injected\": " << invalid_frames_injected << "\n"
           << "}\n";
}

int run(const Options &options)
{
    std::filesystem::create_directories(options.output_dir);
    const auto csv_path = options.output_dir / "samples.csv";
    const auto frame_path = options.output_dir / "telemetry.bin";
    const auto summary_path = options.output_dir / "summary.json";

    std::ofstream csv(csv_path, std::ios::trunc);
    std::ofstream frames(frame_path, std::ios::binary | std::ios::trunc);
    if (!csv || !frames) {
        throw std::runtime_error("Cannot create simulator output files");
    }

    csv << "cycle,timestamp_us,state,fault_flags,motor_enabled,pwm_limit,"
           "acceleration_mps2,yaw_rate_rps,velocity_mps,force_n\n";
    csv << std::fixed << std::setprecision(6);

    rtsafe_pipeline_t pipeline{};
    const auto config = rtsafe_default_config();
    if (!rtsafe_pipeline_init(&pipeline, &config)) {
        throw std::runtime_error("Pipeline initialization failed");
    }

    DeterministicNoise noise(options.seed);
    std::int32_t encoder_ticks = 0;
    std::uint64_t last_heartbeat_us = 0U;
    std::int64_t first_fault_cycle = -1;
    std::int64_t first_safe_stop_cycle = -1;
    std::uint32_t invalid_frames_injected = 0U;
    rtsafe_status_t status{};

    for (std::uint32_t cycle = 0U; cycle < options.cycles; ++cycle) {
        const std::uint64_t now_us =
            static_cast<std::uint64_t>(cycle) * config.sample_period_us;
        if (cycle % 20U == 0U && !(options.scenario == "heartbeat-loss" && cycle >= 900U)) {
            last_heartbeat_us = now_us;
        }

        encoder_ticks += 2;
        const float motion = triangle_wave(cycle, 200U);
        rtsafe_input_t input{};
        input.timestamp_us = now_us;
        input.heartbeat_timestamp_us = last_heartbeat_us;
        input.acceleration_mps2[0] = motion * 1.5F + noise.symmetric(0.03F);
        input.acceleration_mps2[1] = noise.symmetric(0.02F);
        input.acceleration_mps2[2] = 9.81F + noise.symmetric(0.04F);
        input.angular_rate_rps[2] = motion * 0.25F + noise.symmetric(0.005F);
        input.encoder_ticks = encoder_ticks;
        input.force_n = 10.0F + noise.symmetric(0.5F);
        input.imu_valid = true;
        input.encoder_valid = true;
        input.force_valid = true;
        std::uint32_t execution_time_us = 120U + (cycle % 17U);

        apply_scenario(
            options.scenario,
            cycle,
            input,
            execution_time_us,
            last_heartbeat_us
        );

        status = rtsafe_pipeline_step(&pipeline, &input, now_us, execution_time_us);
        if (first_fault_cycle < 0 && status.fault_flags != RTSAFE_FAULT_NONE) {
            first_fault_cycle = cycle;
        }
        if (first_safe_stop_cycle < 0 && status.state == RTSAFE_STATE_SAFE_STOP) {
            first_safe_stop_cycle = cycle;
        }

        std::array<std::uint8_t, RTSAFE_TELEMETRY_FRAME_SIZE> frame{};
        if (!rtsafe_encode_telemetry(&status, frame.data(), frame.size())) {
            throw std::runtime_error("Telemetry encoding failed");
        }
        if (options.scenario == "crc-corruption" && cycle == 700U) {
            frame[27] ^= 0x80U;
            ++invalid_frames_injected;
        }
        frames.write(
            reinterpret_cast<const char *>(frame.data()),
            static_cast<std::streamsize>(frame.size())
        );

        csv << cycle << ',' << status.timestamp_us << ','
            << rtsafe_state_name(status.state) << ',' << status.fault_flags << ','
            << (status.motor_enabled ? 1 : 0) << ',' << status.pwm_limit << ','
            << status.filtered_acceleration_mps2 << ','
            << status.filtered_yaw_rate_rps << ','
            << status.estimated_velocity_mps << ','
            << status.filtered_force_n << '\n';
    }

    write_summary(
        summary_path,
        options,
        status,
        pipeline.safety.degraded_observed,
        pipeline.safety.safe_stop_observed,
        first_fault_cycle,
        first_safe_stop_cycle,
        invalid_frames_injected
    );

    std::cout << "scenario=" << options.scenario
              << " final_state=" << rtsafe_state_name(status.state)
              << " degraded=" << std::boolalpha << pipeline.safety.degraded_observed
              << " safe_stop=" << pipeline.safety.safe_stop_observed
              << " output=" << options.output_dir.string() << '\n';
    return 0;
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

