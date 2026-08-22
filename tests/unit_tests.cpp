#include "rtsafe/crc16.h"
#include "rtsafe/filters.h"
#include "rtsafe/pipeline.h"
#include "rtsafe/protocol.h"
#include "rtsafe/safety.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char *expression, const char *file, int line)
{
    if (!condition) {
        std::cerr << file << ':' << line << " CHECK failed: " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance) \
    check(std::fabs((actual) - (expected)) <= (tolerance), #actual " ~= " #expected, __FILE__, __LINE__)

void test_filters()
{
    rtsafe_lowpass_t lowpass{};
    rtsafe_lowpass_init(&lowpass, 0.5F);
    CHECK_NEAR(rtsafe_lowpass_update(&lowpass, 0.0F), 0.0F, 0.0001F);
    CHECK_NEAR(rtsafe_lowpass_update(&lowpass, 10.0F), 5.0F, 0.0001F);

    rtsafe_moving_average_t average{};
    CHECK(rtsafe_moving_average_init(&average, 3U));
    CHECK_NEAR(rtsafe_moving_average_update(&average, 1.0F), 1.0F, 0.0001F);
    CHECK_NEAR(rtsafe_moving_average_update(&average, 2.0F), 1.5F, 0.0001F);
    CHECK_NEAR(rtsafe_moving_average_update(&average, 3.0F), 2.0F, 0.0001F);
    CHECK_NEAR(rtsafe_moving_average_update(&average, 4.0F), 3.0F, 0.0001F);
    CHECK(!rtsafe_moving_average_init(&average, RTSAFE_MOVING_AVERAGE_CAPACITY + 1U));

    rtsafe_kalman1d_t kalman{};
    rtsafe_kalman1d_init(&kalman, 0.01F, 0.2F);
    CHECK_NEAR(rtsafe_kalman1d_update(&kalman, 2.0F), 2.0F, 0.0001F);
    const float estimate = rtsafe_kalman1d_update(&kalman, 4.0F);
    CHECK(estimate > 2.0F && estimate < 4.0F);
}

void test_crc_and_protocol()
{
    constexpr std::array<std::uint8_t, 9> check_vector = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    CHECK(rtsafe_crc16_ccitt(check_vector.data(), check_vector.size()) == 0x29B1U);

    rtsafe_status_t original{};
    original.sequence = 42U;
    original.timestamp_us = 123456789U;
    original.state = RTSAFE_STATE_DEGRADED;
    original.fault_flags = RTSAFE_FAULT_IMU_INVALID;
    original.filtered_acceleration_mps2 = 9.82F;
    original.filtered_yaw_rate_rps = -0.25F;
    original.estimated_velocity_mps = 1.5F;
    original.filtered_force_n = 11.0F;
    original.pwm_limit = 0.35F;
    original.motor_enabled = true;
    original.deadline_misses = 3U;

    std::array<std::uint8_t, RTSAFE_TELEMETRY_FRAME_SIZE> frame{};
    CHECK(rtsafe_encode_telemetry(&original, frame.data(), frame.size()));
    rtsafe_status_t decoded{};
    CHECK(rtsafe_decode_telemetry(frame.data(), frame.size(), &decoded));
    CHECK(decoded.sequence == original.sequence);
    CHECK(decoded.timestamp_us == original.timestamp_us);
    CHECK(decoded.state == original.state);
    CHECK(decoded.fault_flags == original.fault_flags);
    CHECK_NEAR(decoded.filtered_acceleration_mps2, original.filtered_acceleration_mps2, 0.0001F);
    CHECK_NEAR(decoded.filtered_yaw_rate_rps, original.filtered_yaw_rate_rps, 0.0001F);
    CHECK_NEAR(decoded.estimated_velocity_mps, original.estimated_velocity_mps, 0.0001F);
    CHECK(decoded.motor_enabled == original.motor_enabled);

    frame[30] ^= 0x01U;
    CHECK(!rtsafe_decode_telemetry(frame.data(), frame.size(), &decoded));

    original.pwm_limit = 2.0F;
    CHECK(!rtsafe_encode_telemetry(&original, frame.data(), frame.size()));

    original.pwm_limit = 0.35F;
    CHECK(rtsafe_encode_telemetry(&original, frame.data(), frame.size()));
    frame[20] = 1U;
    const std::uint16_t reserved_crc = rtsafe_crc16_ccitt(frame.data(), frame.size() - 2U);
    frame[50] = static_cast<std::uint8_t>(reserved_crc & 0xFFU);
    frame[51] = static_cast<std::uint8_t>((reserved_crc >> 8U) & 0xFFU);
    CHECK(!rtsafe_decode_telemetry(frame.data(), frame.size(), &decoded));
}

void test_safety_state_machine()
{
    const auto config = rtsafe_default_config();
    rtsafe_safety_t safety{};
    rtsafe_safety_init(&safety);

    for (std::uint32_t index = 0U; index < config.healthy_samples_to_normal; ++index) {
        rtsafe_safety_update(&safety, RTSAFE_FAULT_NONE, &config);
    }
    CHECK(safety.state == RTSAFE_STATE_NORMAL);

    rtsafe_safety_update(&safety, RTSAFE_FAULT_IMU_INVALID, &config);
    CHECK(safety.state == RTSAFE_STATE_DEGRADED);
    CHECK(safety.degraded_observed);

    for (std::uint32_t index = 0U; index < config.healthy_samples_to_normal; ++index) {
        rtsafe_safety_update(&safety, RTSAFE_FAULT_NONE, &config);
    }
    CHECK(safety.state == RTSAFE_STATE_NORMAL);

    rtsafe_safety_update(&safety, RTSAFE_FAULT_FORCE_LIMIT, &config);
    CHECK(safety.state == RTSAFE_STATE_SAFE_STOP);
    CHECK(safety.safe_stop_observed);

    for (std::uint32_t index = 0U; index < config.recovery_samples; ++index) {
        rtsafe_safety_update(&safety, RTSAFE_FAULT_NONE, &config);
    }
    CHECK(safety.state == RTSAFE_STATE_RECOVERY);
    for (std::uint32_t index = 0U; index < config.recovery_samples; ++index) {
        rtsafe_safety_update(&safety, RTSAFE_FAULT_NONE, &config);
    }
    CHECK(safety.state == RTSAFE_STATE_NORMAL);
}

rtsafe_input_t nominal_input(std::uint32_t cycle)
{
    rtsafe_input_t input{};
    input.timestamp_us = static_cast<std::uint64_t>(cycle) * 1000U;
    input.heartbeat_timestamp_us = input.timestamp_us;
    input.acceleration_mps2[2] = 9.81F;
    input.angular_rate_rps[2] = 0.1F;
    input.encoder_ticks = static_cast<std::int32_t>(cycle * 2U);
    input.force_n = 10.0F;
    input.imu_valid = true;
    input.encoder_valid = true;
    input.force_valid = true;
    return input;
}

void test_pipeline_faults()
{
    rtsafe_pipeline_t pipeline{};
    CHECK(rtsafe_pipeline_init(&pipeline, nullptr));
    rtsafe_status_t status{};
    for (std::uint32_t cycle = 0U; cycle < 20U; ++cycle) {
        auto input = nominal_input(cycle);
        status = rtsafe_pipeline_step(&pipeline, &input, input.timestamp_us, 100U);
    }
    CHECK(status.state == RTSAFE_STATE_NORMAL);
    CHECK(status.motor_enabled);
    CHECK_NEAR(status.pwm_limit, 1.0F, 0.0001F);

    auto dropout = nominal_input(20U);
    dropout.imu_valid = false;
    status = rtsafe_pipeline_step(&pipeline, &dropout, dropout.timestamp_us, 100U);
    CHECK((status.fault_flags & RTSAFE_FAULT_IMU_INVALID) != 0U);
    CHECK(status.state == RTSAFE_STATE_DEGRADED);
    CHECK(status.motor_enabled);

    auto force = nominal_input(21U);
    force.force_n = 150.0F;
    status = rtsafe_pipeline_step(&pipeline, &force, force.timestamp_us, 100U);
    CHECK((status.fault_flags & RTSAFE_FAULT_FORCE_LIMIT) != 0U);
    CHECK(status.state == RTSAFE_STATE_SAFE_STOP);
    CHECK(!status.motor_enabled);
    CHECK_NEAR(status.pwm_limit, 0.0F, 0.0001F);
}

} // namespace

int main()
{
    test_filters();
    test_crc_and_protocol();
    test_safety_state_machine();
    test_pipeline_faults();

    if (failures != 0) {
        std::cerr << failures << " unit test assertion(s) failed\n";
        return 1;
    }
    std::cout << "All unit tests passed\n";
    return 0;
}
