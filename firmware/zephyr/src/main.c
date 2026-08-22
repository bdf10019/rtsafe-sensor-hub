#include "board_io.h"
#include "rtsafe/pipeline.h"
#include "rtsafe/protocol.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <stdint.h>

#define SENSOR_STACK_SIZE 1536
#define CONTROL_STACK_SIZE 2048
#define TELEMETRY_STACK_SIZE 1536

K_MSGQ_DEFINE(sensor_queue, sizeof(rtsafe_input_t), 16, 4);
K_MSGQ_DEFINE(status_queue, sizeof(rtsafe_status_t), 16, 4);

static rtsafe_pipeline_t pipeline;
static uint32_t previous_execution_us;

static void sensor_thread(void *first, void *second, void *third)
{
    ARG_UNUSED(first);
    ARG_UNUSED(second);
    ARG_UNUSED(third);

    while (true) {
        rtsafe_input_t input;
        if (rtsafe_board_read_sensors(&input)) {
            if (k_msgq_put(&sensor_queue, &input, K_NO_WAIT) != 0) {
                printk("sensor_queue_overflow\n");
            }
        }
        k_sleep(K_USEC(1000));
    }
}

static void control_thread(void *first, void *second, void *third)
{
    ARG_UNUSED(first);
    ARG_UNUSED(second);
    ARG_UNUSED(third);

    while (true) {
        rtsafe_input_t input;
        if (k_msgq_get(&sensor_queue, &input, K_FOREVER) == 0) {
            const uint32_t started = k_cycle_get_32();
            rtsafe_status_t status = rtsafe_pipeline_step(
                &pipeline,
                &input,
                input.timestamp_us,
                previous_execution_us
            );
            rtsafe_board_apply_output(&status);
            if (k_msgq_put(&status_queue, &status, K_NO_WAIT) != 0) {
                printk("status_queue_overflow\n");
            }
            previous_execution_us = (uint32_t)k_cyc_to_us_floor64(
                (uint64_t)(k_cycle_get_32() - started)
            );
        }
    }
}

static void telemetry_thread(void *first, void *second, void *third)
{
    ARG_UNUSED(first);
    ARG_UNUSED(second);
    ARG_UNUSED(third);

    while (true) {
        rtsafe_status_t status;
        uint8_t frame[RTSAFE_TELEMETRY_FRAME_SIZE];
        if (k_msgq_get(&status_queue, &status, K_FOREVER) == 0 &&
            rtsafe_encode_telemetry(&status, frame, sizeof(frame))) {
            rtsafe_board_publish_frame(frame, sizeof(frame), status.sequence);
        }
    }
}

K_THREAD_DEFINE(
    sensor_thread_id,
    SENSOR_STACK_SIZE,
    sensor_thread,
    NULL,
    NULL,
    NULL,
    2,
    0,
    20
);
K_THREAD_DEFINE(
    control_thread_id,
    CONTROL_STACK_SIZE,
    control_thread,
    NULL,
    NULL,
    NULL,
    1,
    0,
    20
);
K_THREAD_DEFINE(
    telemetry_thread_id,
    TELEMETRY_STACK_SIZE,
    telemetry_thread,
    NULL,
    NULL,
    NULL,
    5,
    0,
    20
);

int main(void)
{
    const rtsafe_config_t config = rtsafe_default_config();
    if (!rtsafe_pipeline_init(&pipeline, &config)) {
        printk("RTSAFE_INIT_FAILED\n");
        return 1;
    }

    printk("RTSAFE_READY sample_period_us=%u budget_us=%u\n",
        config.sample_period_us,
        config.execution_budget_us);
    return 0;
}
