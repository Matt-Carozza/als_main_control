#include <stdio.h>
#include "main_control.h"
#include "transport_mqtt.h"
#include "message_router.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "MAIN";

#define DAYLIGHT_MIN_VOLTAGE 0.15f
#define DAYLIGHT_MAX_VOLTAGE 2.0f
#define FLOOR_BRIGHTNESS 0.20f

void main_control_handle(const QueueMessage *msg) {
    switch (msg->main.action)
    {
        case MAIN_HEARTBEAT_UPDATE:        
            break;
            
        case MAIN_OCCUPANCY_UPDATE:

            QueueMessage light_msg = {
                .origin = ORIGIN_MAIN,
                .device = DEVICE_LIGHT,
                .light.action = LIGHT_SET_RGB
            };

            if (msg->main.payload.occupancy_update.occupied) {
                light_msg.light.payload.set_rgb.r = 255;
                light_msg.light.payload.set_rgb.g = 255;
                light_msg.light.payload.set_rgb.b = 255;
                light_msg.light.payload.set_rgb.room_id = 
                    msg->main.payload.occupancy_update.room_id;
                ESP_LOGI(TAG, "Room occupied again, turning on lights");
            } else {
                light_msg.light.payload.set_rgb.r = 0;
                light_msg.light.payload.set_rgb.g = 0;
                light_msg.light.payload.set_rgb.b = 0;
                light_msg.light.payload.set_rgb.room_id = 
                    msg->main.payload.occupancy_update.room_id;
                ESP_LOGI(TAG, "Room is unoccupied, turning off lights");
            }
            
            if (message_router_push_local(&light_msg) != pdPASS) { 
                ESP_LOGE("STATUS_TASK", "Failed to send message to queue");
            } 
            break;
        case MAIN_TOGGLE_ADAPTIVE_LIGHTING_MODE:
            ESP_LOGI(TAG, "MAIN_TOGGLE_ADAPTIVE_LIGHTING_MODE: %u", 
                msg->main.payload.toggle_adaptive_lighting_mode.room_id);
            if (msg->main.payload.toggle_adaptive_lighting_mode.enabled) {
                ESP_LOGI(TAG, "Wake Time: %s", 
                    msg->main.payload.toggle_adaptive_lighting_mode.wake_time);
                ESP_LOGI(TAG, "Sleep Time: %s",
                    msg->main.payload.toggle_adaptive_lighting_mode.sleep_time);
            }
            break;
        case MAIN_DAY_UPDATE:
            ESP_LOGI(TAG, "MAIN_DAY_UPDATE voltage: %f", 
                msg->main.payload.day_update.voltage);

            float normalized_daylight_ratio = ((msg->main.payload.day_update.voltage - 0.15) / DAYLIGHT_MAX_VOLTAGE);
            if (normalized_daylight_ratio < 0) normalized_daylight_ratio = 0;
            if (normalized_daylight_ratio > 1) normalized_daylight_ratio = 1;

            float inverse_daylight_ratio = 1 - normalized_daylight_ratio;
            float brightness = (1 - FLOOR_BRIGHTNESS) * inverse_daylight_ratio + FLOOR_BRIGHTNESS; 
            ESP_LOGI(TAG, "Changing light brightness in room %u by %f", 
                msg->main.payload.day_update.room_id, brightness);
        default:
            break;
    }
}