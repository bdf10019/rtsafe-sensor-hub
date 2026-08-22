#include "rtsafe/filters.h"

#include <string.h>

void rtsafe_lowpass_init(rtsafe_lowpass_t *filter, float alpha)
{
    if (filter == NULL) {
        return;
    }

    if (alpha < 0.0F) {
        alpha = 0.0F;
    } else if (alpha > 1.0F) {
        alpha = 1.0F;
    }

    filter->alpha = alpha;
    filter->value = 0.0F;
    filter->initialized = false;
}

float rtsafe_lowpass_update(rtsafe_lowpass_t *filter, float sample)
{
    if (filter == NULL) {
        return sample;
    }

    if (!filter->initialized) {
        filter->value = sample;
        filter->initialized = true;
    } else {
        filter->value += filter->alpha * (sample - filter->value);
    }

    return filter->value;
}

bool rtsafe_moving_average_init(rtsafe_moving_average_t *filter, size_t window)
{
    if (filter == NULL || window == 0U || window > RTSAFE_MOVING_AVERAGE_CAPACITY) {
        return false;
    }

    memset(filter, 0, sizeof(*filter));
    filter->window = window;
    return true;
}

float rtsafe_moving_average_update(rtsafe_moving_average_t *filter, float sample)
{
    if (filter == NULL || filter->window == 0U) {
        return sample;
    }

    if (filter->count < filter->window) {
        ++filter->count;
    } else {
        filter->sum -= filter->values[filter->next];
    }

    filter->values[filter->next] = sample;
    filter->sum += sample;
    filter->next = (filter->next + 1U) % filter->window;

    return filter->sum / (float)filter->count;
}

void rtsafe_kalman1d_init(
    rtsafe_kalman1d_t *filter,
    float process_noise,
    float measurement_noise
)
{
    if (filter == NULL) {
        return;
    }

    filter->estimate = 0.0F;
    filter->error_covariance = 1.0F;
    filter->process_noise = process_noise > 0.0F ? process_noise : 0.001F;
    filter->measurement_noise = measurement_noise > 0.0F ? measurement_noise : 0.1F;
    filter->initialized = false;
}

float rtsafe_kalman1d_update(rtsafe_kalman1d_t *filter, float measurement)
{
    float gain;

    if (filter == NULL) {
        return measurement;
    }

    if (!filter->initialized) {
        filter->estimate = measurement;
        filter->initialized = true;
        return filter->estimate;
    }

    filter->error_covariance += filter->process_noise;
    gain = filter->error_covariance /
        (filter->error_covariance + filter->measurement_noise);
    filter->estimate += gain * (measurement - filter->estimate);
    filter->error_covariance *= 1.0F - gain;
    return filter->estimate;
}

