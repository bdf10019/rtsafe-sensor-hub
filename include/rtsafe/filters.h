#ifndef RTSAFE_FILTERS_H
#define RTSAFE_FILTERS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTSAFE_MOVING_AVERAGE_CAPACITY 16U

typedef struct {
    float alpha;
    float value;
    bool initialized;
} rtsafe_lowpass_t;

typedef struct {
    float values[RTSAFE_MOVING_AVERAGE_CAPACITY];
    size_t window;
    size_t count;
    size_t next;
    float sum;
} rtsafe_moving_average_t;

typedef struct {
    float estimate;
    float error_covariance;
    float process_noise;
    float measurement_noise;
    bool initialized;
} rtsafe_kalman1d_t;

void rtsafe_lowpass_init(rtsafe_lowpass_t *filter, float alpha);
float rtsafe_lowpass_update(rtsafe_lowpass_t *filter, float sample);

bool rtsafe_moving_average_init(rtsafe_moving_average_t *filter, size_t window);
float rtsafe_moving_average_update(rtsafe_moving_average_t *filter, float sample);

void rtsafe_kalman1d_init(
    rtsafe_kalman1d_t *filter,
    float process_noise,
    float measurement_noise
);
float rtsafe_kalman1d_update(rtsafe_kalman1d_t *filter, float measurement);

#ifdef __cplusplus
}
#endif

#endif

