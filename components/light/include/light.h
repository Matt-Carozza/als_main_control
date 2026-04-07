#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

void light_handle(const QueueMessage *msg);
RGB light_base_color_get(uint8_t room_id);