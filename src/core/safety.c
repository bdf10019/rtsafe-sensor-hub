#include "rtsafe/safety.h"

#include <stddef.h>

static bool has_critical_fault(uint32_t fault_flags)
{
    const uint32_t critical_mask =
        RTSAFE_FAULT_FORCE_INVALID |
        RTSAFE_FAULT_HEARTBEAT_LOST |
        RTSAFE_FAULT_FORCE_LIMIT |
        RTSAFE_FAULT_TIME_REGRESSION;

    return (fault_flags & critical_mask) != 0U;
}

const char *rtsafe_state_name(rtsafe_state_t state)
{
    switch (state) {
    case RTSAFE_STATE_INIT:
        return "INIT";
    case RTSAFE_STATE_NORMAL:
        return "NORMAL";
    case RTSAFE_STATE_DEGRADED:
        return "DEGRADED";
    case RTSAFE_STATE_SAFE_STOP:
        return "SAFE_STOP";
    case RTSAFE_STATE_RECOVERY:
        return "RECOVERY";
    default:
        return "UNKNOWN";
    }
}

void rtsafe_safety_init(rtsafe_safety_t *safety)
{
    if (safety == NULL) {
        return;
    }

    safety->state = RTSAFE_STATE_INIT;
    safety->healthy_streak = 0U;
    safety->fault_streak = 0U;
    safety->degraded_observed = false;
    safety->safe_stop_observed = false;
}

rtsafe_state_t rtsafe_safety_update(
    rtsafe_safety_t *safety,
    uint32_t fault_flags,
    const rtsafe_config_t *config
)
{
    const bool critical = has_critical_fault(fault_flags);
    const bool healthy = fault_flags == RTSAFE_FAULT_NONE;

    if (safety == NULL || config == NULL) {
        return RTSAFE_STATE_SAFE_STOP;
    }

    if (healthy) {
        ++safety->healthy_streak;
        safety->fault_streak = 0U;
    } else {
        safety->healthy_streak = 0U;
        ++safety->fault_streak;
    }

    switch (safety->state) {
    case RTSAFE_STATE_INIT:
        if (critical) {
            safety->state = RTSAFE_STATE_SAFE_STOP;
        } else if (!healthy) {
            safety->state = RTSAFE_STATE_DEGRADED;
        } else if (safety->healthy_streak >= config->healthy_samples_to_normal) {
            safety->state = RTSAFE_STATE_NORMAL;
        }
        break;

    case RTSAFE_STATE_NORMAL:
        if (critical) {
            safety->state = RTSAFE_STATE_SAFE_STOP;
        } else if (!healthy) {
            safety->state = RTSAFE_STATE_DEGRADED;
        }
        break;

    case RTSAFE_STATE_DEGRADED:
        if (critical || safety->fault_streak >= config->max_degraded_cycles) {
            safety->state = RTSAFE_STATE_SAFE_STOP;
        } else if (healthy &&
                   safety->healthy_streak >= config->healthy_samples_to_normal) {
            safety->state = RTSAFE_STATE_NORMAL;
        }
        break;

    case RTSAFE_STATE_SAFE_STOP:
        if (healthy && safety->healthy_streak >= config->recovery_samples) {
            safety->state = RTSAFE_STATE_RECOVERY;
            safety->healthy_streak = 0U;
        }
        break;

    case RTSAFE_STATE_RECOVERY:
        if (critical) {
            safety->state = RTSAFE_STATE_SAFE_STOP;
        } else if (!healthy) {
            safety->state = RTSAFE_STATE_DEGRADED;
        } else if (safety->healthy_streak >= config->recovery_samples) {
            safety->state = RTSAFE_STATE_NORMAL;
        }
        break;

    default:
        safety->state = RTSAFE_STATE_SAFE_STOP;
        break;
    }

    if (safety->state == RTSAFE_STATE_DEGRADED) {
        safety->degraded_observed = true;
    }
    if (safety->state == RTSAFE_STATE_SAFE_STOP) {
        safety->safe_stop_observed = true;
    }

    return safety->state;
}

