#include <stdio.h>
#include "main_control.h"
#include "transport_mqtt.h"
#include "message_router.h"
#include "freertos/FreeRTOS.h"
#include "light.h"

static const char* TAG = "MAIN";

#define DAYLIGHT_MIN_VOLTAGE 0.15f
#define DAYLIGHT_MAX_VOLTAGE 2.0f
#define FLOOR_BRIGHTNESS 0.20f

void main_control_handle(const QueueMessage *msg) {
    switch (msg->main.action)
    {
        case MAIN_HEARTBEAT_UPDATE:        
            break;
            
        case MAIN_OCCUPANCY_UPDATE: {
            QueueMessage light_msg = {
                .origin = ORIGIN_MAIN,
                .device = DEVICE_LIGHT,
                .light.action = LIGHT_UPDATE_STATE,
                .light.payload.state_update = {
                    .room_id = msg->main.payload.occupancy_update.room_id,
                    .has_enabled = true
                }
            };

            if (msg->main.payload.occupancy_update.occupied) {
                light_msg.light.payload.state_update.enabled = true;
                ESP_LOGD(TAG, "Room occupied again, turning on lights");
            } else {
                light_msg.light.payload.state_update.enabled = false;
                ESP_LOG(TAG, "Room is unoccupied, turning off lights");
            }
            
            if (message_router_push_local(&light_msg) != pdPASS) { 
                ESP_LOGE(TAG, "Failed to send message to queue");
            } 
            break;
        }

        case MAIN_TOGGLE_ADAPTIVE_LIGHTING_MODE: {
            ESP_LOGI(TAG, "MAIN_TOGGLE_ADAPTIVE_LIGHTING_MODE: %u", 
                msg->main.payload.toggle_adaptive_lighting_mode.room_id);
            if (msg->main.payload.toggle_adaptive_lighting_mode.enabled) {
                ESP_LOGI(TAG, "Wake Time: %s", 
                    msg->main.payload.toggle_adaptive_lighting_mode.wake_time);
                ESP_LOGI(TAG, "Sleep Time: %s",
                    msg->main.payload.toggle_adaptive_lighting_mode.sleep_time);
            }
            break;
        }
        case MAIN_DAY_UPDATE: {
            ESP_LOGD(TAG, "MAIN_DAY_UPDATE voltage: %f", 
                msg->main.payload.day_update.voltage);

            float normalized = ((msg->main.payload.day_update.voltage - DAYLIGHT_MIN_VOLTAGE) /
                                                (DAYLIGHT_MAX_VOLTAGE - DAYLIGHT_MIN_VOLTAGE));

            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;

            float inverse_daylight_ratio = 1.0f - normalized;
            float brightness = (1.0f - FLOOR_BRIGHTNESS) * inverse_daylight_ratio +
                                FLOOR_BRIGHTNESS; 
            ESP_LOGd(TAG, "Changing light brightness in room %u by %.1f%%", 
                msg->main.payload.day_update.room_id, brightness * 100.0f);
            
            QueueMessage light_msg = {
                .origin = ORIGIN_MAIN,
                .device = DEVICE_LIGHT,
                .light.action = LIGHT_UPDATE_STATE,
                .light.payload.state_update = {
                    .room_id = msg->main.payload.day_update.room_id,
                    .has_brightness = true,
                    .room_brightness = brightness
                }
            };

            if (message_router_push_local(&light_msg) != pdPASS) {
                ESP_LOGE(TAG, "Failed to send message to queue");
            }
            break;
        }

        default:
            break;
    }
}