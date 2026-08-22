#ifndef RTSAFE_SAFETY_H
#define RTSAFE_SAFETY_H

#include "rtsafe/types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    rtsafe_state_t state;
    uint32_t healthy_streak;
    uint32_t fault_streak;
    bool degraded_observed;
    bool safe_stop_observed;
} rtsafe_safety_t;

void rtsafe_safety_init(rtsafe_safety_t *safety);
rtsafe_state_t rtsafe_safety_update(
    rtsafe_safety_t *safety,
    uint32_t fault_flags,
    const rtsafe_config_t *config
);

#ifdef __cplusplus
}
#endif

#endif

