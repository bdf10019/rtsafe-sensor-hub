#include "board_io.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <string.h>

bool rtsafe_board_read_sensors(rtsafe_input_t *input)
{
    static int32_t encoder_ticks;
    static uint32_t sample_count;
    static uint64_t last_heartbeat_us;
    const uint64_t now_us = (uint64_t)k_uptime_get() * 1000U;
    int32_t phase;

    if (input == NULL) {
        return false;
    }

    memset(input, 0, sizeof(*input));
    encoder_ticks += 2;
    phase = (int32_t)(sample_count % 200U) - 100;
    if (sample_count % 20U == 0U) {
        last_heartbeat_us = now_us;
    }

    input->timestamp_us = now_us;
    input->heartbeat_timestamp_us = last_heartbeat_us;
    input->acceleration_mps2[0] = (float)phase * 0.01F;
    input->acceleration_mps2[2] = 9.81F;
    input->angular_rate_rps[2] = (float)phase * 0.001F;
    input->encoder_ticks = encoder_ticks;
    input->force_n = 10.0F;
    input->imu_valid = true;
    input->encoder_valid = true;
    input->force_valid = true;
    ++sample_count;
    return true;
}

void rtsafe_board_apply_output(const rtsafe_status_t *status)
{
    ARG_UNUSED(status);
    /* A real board port maps motor_enabled and pwm_limit to a fail-safe output. */
}

void rtsafe_board_publish_frame(const uint8_t *frame, size_t size, uint32_t sequence)
{
    ARG_UNUSED(frame);
    ARG_UNUSED(size);
    if (sequence % 250U == 0U) {
        printk("telemetry sequence=%u\n", sequence);
    }
}

