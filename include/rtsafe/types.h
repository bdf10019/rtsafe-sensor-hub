#ifndef RTSAFE_TYPES_H
#define RTSAFE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RTSAFE_STATE_INIT = 0,
    RTSAFE_STATE_NORMAL = 1,
    RTSAFE_STATE_DEGRADED = 2,
    RTSAFE_STATE_SAFE_STOP = 3,
    RTSAFE_STATE_RECOVERY = 4
} rtsafe_state_t;

enum {
    RTSAFE_FAULT_NONE = 0U,
    RTSAFE_FAULT_IMU_INVALID = 1U << 0,
    RTSAFE_FAULT_ENCODER_INVALID = 1U << 1,
    RTSAFE_FAULT_FORCE_INVALID = 1U << 2,
    RTSAFE_FAULT_HEARTBEAT_LOST = 1U << 3,
    RTSAFE_FAULT_FORCE_LIMIT = 1U << 4,
    RTSAFE_FAULT_ENCODER_JUMP = 1U << 5,
    RTSAFE_FAULT_DEADLINE_MISS = 1U << 6,
    RTSAFE_FAULT_TIME_REGRESSION = 1U << 7,
    RTSAFE_FAULT_IMU_RANGE = 1U << 8
};

typedef struct {
    uint64_t timestamp_us;
    uint64_t heartbeat_timestamp_us;
    float acceleration_mps2[3];
    float angular_rate_rps[3];
    int32_t encoder_ticks;
    float force_n;
    bool imu_valid;
    bool encoder_valid;
    bool force_valid;
} rtsafe_input_t;

typedef struct {
    uint32_t sample_period_us;
    uint32_t execution_budget_us;
    uint32_t heartbeat_timeout_us;
    float force_stop_n;
    float max_acceleration_mps2;
    float max_angular_rate_rps;
    int32_t max_encoder_delta_ticks;
    float meters_per_tick;
    float lowpass_alpha;
    float degraded_pwm_limit;
    uint32_t healthy_samples_to_normal;
    uint32_t recovery_samples;
    uint32_t max_degraded_cycles;
} rtsafe_config_t;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    rtsafe_state_t state;
    uint32_t fault_flags;
    float filtered_acceleration_mps2;
    float filtered_yaw_rate_rps;
    float estimated_velocity_mps;
    float filtered_force_n;
    float pwm_limit;
    bool motor_enabled;
    uint32_t deadline_misses;
} rtsafe_status_t;

const char *rtsafe_state_name(rtsafe_state_t state);

#ifdef __cplusplus
}
#endif

#endif

