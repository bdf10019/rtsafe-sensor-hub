#ifndef RTSAFE_PROTOCOL_H
#define RTSAFE_PROTOCOL_H

#include "rtsafe/types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTSAFE_PROTOCOL_VERSION 1U
#define RTSAFE_FRAME_TYPE_TELEMETRY 1U
#define RTSAFE_TELEMETRY_PAYLOAD_SIZE 32U
#define RTSAFE_TELEMETRY_FRAME_SIZE 52U

bool rtsafe_encode_telemetry(
    const rtsafe_status_t *status,
    uint8_t *destination,
    size_t destination_size
);

bool rtsafe_decode_telemetry(
    const uint8_t *frame,
    size_t frame_size,
    rtsafe_status_t *status
);

#ifdef __cplusplus
}
#endif

#endif

