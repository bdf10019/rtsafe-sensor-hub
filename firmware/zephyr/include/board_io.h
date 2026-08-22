#ifndef RTSAFE_BOARD_IO_H
#define RTSAFE_BOARD_IO_H

#include "rtsafe/types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool rtsafe_board_read_sensors(rtsafe_input_t *input);
void rtsafe_board_apply_output(const rtsafe_status_t *status);
void rtsafe_board_publish_frame(const uint8_t *frame, size_t size, uint32_t sequence);

#endif

