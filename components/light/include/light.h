#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

typedef struct {
    uint8_t r, g, b;
} RGB;

void light_handle(const QueueMessage *msg);