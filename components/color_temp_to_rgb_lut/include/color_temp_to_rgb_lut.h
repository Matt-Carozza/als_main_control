#pragma once
#include "light.h" // Gives RGB from protocol to here. Could be changed to just protocol

#define MIN_COLOR_TEMP 2700U 
#define MAX_COLOR_TEMP 6500U

typedef struct {
    RGB value;
    bool success;
} RGBResult;

RGBResult color_temp_to_rgb(uint16_t color_temp);
