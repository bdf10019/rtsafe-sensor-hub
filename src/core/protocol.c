#include "rtsafe/protocol.h"

#include "rtsafe/crc16.h"

#include <math.h>
#include <string.h>

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void write_u64_le(uint8_t *destination, uint64_t value)
{
    uint8_t index;
    for (index = 0U; index < 8U; ++index) {
        destination[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static void write_f32_le(uint8_t *destination, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(destination, bits);
}

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8U);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

static uint64_t read_u64_le(const uint8_t *source)
{
    uint64_t value = 0U;
    uint8_t index;
    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)source[index] << (index * 8U);
    }
    return value;
}

static float read_f32_le(const uint8_t *source)
{
    const uint32_t bits = read_u32_le(source);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

bool rtsafe_encode_telemetry(
    const rtsafe_status_t *status,
    uint8_t *destination,
    size_t destination_size
)
{
    uint16_t crc;

    if (status == NULL || destination == NULL ||
        destination_size < RTSAFE_TELEMETRY_FRAME_SIZE ||
        status->state < RTSAFE_STATE_INIT || status->state > RTSAFE_STATE_RECOVERY ||
        !isfinite(status->filtered_acceleration_mps2) ||
        !isfinite(status->filtered_yaw_rate_rps) ||
        !isfinite(status->estimated_velocity_mps) ||
        !isfinite(status->filtered_force_n) ||
        !isfinite(status->pwm_limit) ||
        status->pwm_limit < 0.0F || status->pwm_limit > 1.0F) {
        return false;
    }

    memset(destination, 0, RTSAFE_TELEMETRY_FRAME_SIZE);
    destination[0] = (uint8_t)'R';
    destination[1] = (uint8_t)'T';
    destination[2] = RTSAFE_PROTOCOL_VERSION;
    destination[3] = RTSAFE_FRAME_TYPE_TELEMETRY;
    write_u16_le(&destination[4], RTSAFE_TELEMETRY_PAYLOAD_SIZE);
    write_u32_le(&destination[6], status->sequence);
    write_u64_le(&destination[10], status->timestamp_us);
    destination[18] = (uint8_t)status->state;
    destination[19] = status->motor_enabled ? 1U : 0U;
    write_u32_le(&destination[22], status->fault_flags);
    write_f32_le(&destination[26], status->filtered_acceleration_mps2);
    write_f32_le(&destination[30], status->filtered_yaw_rate_rps);
    write_f32_le(&destination[34], status->estimated_velocity_mps);
    write_f32_le(&destination[38], status->filtered_force_n);
    write_f32_le(&destination[42], status->pwm_limit);
    write_u32_le(&destination[46], status->deadline_misses);

    crc = rtsafe_crc16_ccitt(destination, RTSAFE_TELEMETRY_FRAME_SIZE - 2U);
    write_u16_le(&destination[50], crc);
    return true;
}

bool rtsafe_decode_telemetry(
    const uint8_t *frame,
    size_t frame_size,
    rtsafe_status_t *status
)
{
    rtsafe_status_t decoded;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if (frame == NULL || status == NULL ||
        frame_size != RTSAFE_TELEMETRY_FRAME_SIZE) {
        return false;
    }
    if (frame[0] != (uint8_t)'R' || frame[1] != (uint8_t)'T' ||
        frame[2] != RTSAFE_PROTOCOL_VERSION ||
        frame[3] != RTSAFE_FRAME_TYPE_TELEMETRY ||
        read_u16_le(&frame[4]) != RTSAFE_TELEMETRY_PAYLOAD_SIZE ||
        frame[18] > (uint8_t)RTSAFE_STATE_RECOVERY ||
        frame[19] > 1U || frame[20] != 0U || frame[21] != 0U) {
        return false;
    }

    expected_crc = read_u16_le(&frame[50]);
    actual_crc = rtsafe_crc16_ccitt(frame, RTSAFE_TELEMETRY_FRAME_SIZE - 2U);
    if (expected_crc != actual_crc) {
        return false;
    }

    memset(&decoded, 0, sizeof(decoded));
    decoded.sequence = read_u32_le(&frame[6]);
    decoded.timestamp_us = read_u64_le(&frame[10]);
    decoded.state = (rtsafe_state_t)frame[18];
    decoded.motor_enabled = frame[19] == 1U;
    decoded.fault_flags = read_u32_le(&frame[22]);
    decoded.filtered_acceleration_mps2 = read_f32_le(&frame[26]);
    decoded.filtered_yaw_rate_rps = read_f32_le(&frame[30]);
    decoded.estimated_velocity_mps = read_f32_le(&frame[34]);
    decoded.filtered_force_n = read_f32_le(&frame[38]);
    decoded.pwm_limit = read_f32_le(&frame[42]);
    decoded.deadline_misses = read_u32_le(&frame[46]);
    if (!isfinite(decoded.filtered_acceleration_mps2) ||
        !isfinite(decoded.filtered_yaw_rate_rps) ||
        !isfinite(decoded.estimated_velocity_mps) ||
        !isfinite(decoded.filtered_force_n) ||
        !isfinite(decoded.pwm_limit) ||
        decoded.pwm_limit < 0.0F || decoded.pwm_limit > 1.0F) {
        return false;
    }

    *status = decoded;
    return true;
}
