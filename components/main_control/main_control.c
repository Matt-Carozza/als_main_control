#include <stdio.h>
#include "main_control.h"
#include "transport_mqtt.h"
#include "message_router.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "MAIN";

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
        default:
            break;
    }
}