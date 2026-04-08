#include <stdio.h>
#include <math.h>
#include "color_temp_to_rgb_lut.h"

#define LUT_SIZE 39U
#define OFFSET (MIN_COLOR_TEMP / 100)

static const char* TAG = "KELVIN_2_RGB_LUT";

static const RGB table[LUT_SIZE] = {
    {255, 169, 87},  // 2700K
    {255, 173, 94},  // 2800K
    {255, 177, 101}, // 2900K
    {255, 180, 107}, // 3000K
    {255, 184, 114}, // 3100K
    {255, 187, 120}, // 3200K
    {255, 190, 126}, // 3300K
    {255, 193, 132}, // 3400K
    {255, 196, 137}, // 3500K
    {255, 199, 143}, // 3600K
    {255, 201, 148}, // 3700K
    {255, 204, 153}, // 3800K
    {255, 206, 159}, // 3900K
    {255, 209, 163}, // 4000K
    {255, 211, 168}, // 4100K
    {255, 213, 173}, // 4200K
    {255, 215, 177}, // 4300K
    {255, 217, 182}, // 4400K
    {255, 219, 186}, // 4500K
    {255, 221, 190}, // 4600K
    {255, 223, 194}, // 4700K
    {255, 225, 198}, // 4800K
    {255, 227, 202}, // 4900K
    {255, 228, 206}, // 5000K
    {255, 230, 210}, // 5100K
    {255, 232, 213}, // 5200K
    {255, 233, 217}, // 5300K
    {255, 235, 220}, // 5400K
    {255, 236, 224}, // 5500K
    {255, 238, 227}, // 5600K
    {255, 239, 230}, // 5700K
    {255, 240, 233}, // 5800K
    {255, 242, 236}, // 5900K
    {255, 243, 239}, // 6000K
    {255, 244, 242}, // 6100K
    {255, 245, 245}, // 6200K
    {255, 246, 247}, // 6300K
    {255, 248, 251}, // 6400K
    {255, 249, 253}  // 6500K
};

static RGBResult rgb_false_result();

static uint8_t gamma_correct(uint8_t value);

RGBResult color_temp_to_rgb(uint16_t color_temp) {
    RGBResult rgb;
    if (color_temp > MAX_COLOR_TEMP || color_temp < MIN_COLOR_TEMP)
        return rgb_false_result();

    uint16_t index = (color_temp / 100) - OFFSET;
    
    if (index > LUT_SIZE)
        return rgb_false_result();

    rgb.success = true;
    rgb.value = table[index];
    
    rgb.value.r = gamma_correct(rgb.value.r);
    rgb.value.g = gamma_correct(rgb.value.g);
    rgb.value.b = gamma_correct(rgb.value.b);

    return rgb;
}

static RGBResult rgb_false_result() {
    ESP_LOGE(TAG, "Failed lookup request");
    return (RGBResult) {
        .success = false
    };
}

static uint8_t gamma_correct(uint8_t value) {
    return (uint8_t)(pow(value / 255.0, 2.2) * 255);
}