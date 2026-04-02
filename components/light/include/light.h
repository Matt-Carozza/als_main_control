#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

typedef struct {
    uint8_t r, g, b;
} RGB;

void light_handle(const QueueMessage *msg);
RGB light_base_color_get(uint8_t room_id);