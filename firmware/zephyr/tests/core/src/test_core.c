#include "rtsafe/crc16.h"
#include "rtsafe/pipeline.h"
#include "rtsafe/protocol.h"

#include <zephyr/ztest.h>

#include <string.h>

ZTEST(rtsafe_core, test_crc_reference_vector)
{
    static const uint8_t vector[] = "123456789";
    zassert_equal(rtsafe_crc16_ccitt(vector, sizeof(vector) - 1U), 0x29B1U);
}

ZTEST(rtsafe_core, test_protocol_rejects_corruption)
{
    rtsafe_status_t source;
    rtsafe_status_t decoded;
    uint8_t frame[RTSAFE_TELEMETRY_FRAME_SIZE];

    memset(&source, 0, sizeof(source));
    source.sequence = 7U;
    source.timestamp_us = 1000U;
    source.state = RTSAFE_STATE_NORMAL;
    source.motor_enabled = true;
    source.pwm_limit = 1.0F;
    zassert_true(rtsafe_encode_telemetry(&source, frame, sizeof(frame)));
    zassert_true(rtsafe_decode_telemetry(frame, sizeof(frame), &decoded));
    zassert_equal(decoded.sequence, source.sequence);

    frame[26] ^= 1U;
    zassert_false(rtsafe_decode_telemetry(frame, sizeof(frame), &decoded));
}

ZTEST(rtsafe_core, test_critical_force_enters_safe_stop)
{
    rtsafe_pipeline_t pipeline;
    rtsafe_input_t input;
    rtsafe_status_t status;
    uint32_t cycle;

    zassert_true(rtsafe_pipeline_init(&pipeline, NULL));
    for (cycle = 0U; cycle < 20U; ++cycle) {
        memset(&input, 0, sizeof(input));
        input.timestamp_us = (uint64_t)cycle * 1000U;
        input.heartbeat_timestamp_us = input.timestamp_us;
        input.acceleration_mps2[2] = 9.81F;
        input.encoder_ticks = (int32_t)cycle * 2;
        input.force_n = 10.0F;
        input.imu_valid = true;
        input.encoder_valid = true;
        input.force_valid = true;
        status = rtsafe_pipeline_step(
            &pipeline,
            &input,
            input.timestamp_us,
            100U
        );
    }
    zassert_equal(status.state, RTSAFE_STATE_NORMAL);

    input.timestamp_us += 1000U;
    input.heartbeat_timestamp_us = input.timestamp_us;
    input.force_n = 150.0F;
    status = rtsafe_pipeline_step(&pipeline, &input, input.timestamp_us, 100U);
    zassert_equal(status.state, RTSAFE_STATE_SAFE_STOP);
    zassert_false(status.motor_enabled);
}

ZTEST_SUITE(rtsafe_core, NULL, NULL, NULL, NULL, NULL);

