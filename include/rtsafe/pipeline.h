#ifndef RTSAFE_PIPELINE_H
#define RTSAFE_PIPELINE_H

#include "rtsafe/filters.h"
#include "rtsafe/safety.h"
#include "rtsafe/types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    rtsafe_config_t config;
    rtsafe_safety_t safety;
    rtsafe_lowpass_t acceleration_filter;
    rtsafe_lowpass_t yaw_rate_filter;
    rtsafe_moving_average_t force_filter;
    rtsafe_kalman1d_t velocity_filter;
    uint64_t previous_timestamp_us;
    int32_t previous_encoder_ticks;
    uint32_t sequence;
    uint32_t deadline_misses;
    bool has_previous_sample;
} rtsafe_pipeline_t;

rtsafe_config_t rtsafe_default_config(void);
bool rtsafe_pipeline_init(rtsafe_pipeline_t *pipeline, const rtsafe_config_t *config);
rtsafe_status_t rtsafe_pipeline_step(
    rtsafe_pipeline_t *pipeline,
    const rtsafe_input_t *input,
    uint64_t now_us,
    uint32_t execution_time_us
);

#ifdef __cplusplus
}
#endif

#endif

