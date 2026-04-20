#include <stdio.h>
#include "mobile_app.h"
#include "transport_mqtt.h"
#include "adaptive_lighting_mode.h"
#include "message_router.h"
#include "freertos/FreeRTOS.h"

#define LAYING_BRIGHTNESS 0.2f

static const char* TAG = "MOBILE_APP";

static bool get_room_state(uint8_t room_id, RoomState *out);
static void log_room_state(const RoomState *state);

// IR Camera
typedef struct {
    uint16_t presence_misses;
    uint8_t laying_hits;
    uint8_t laying_misses;
    bool laying_detected;
    bool any_presence;
} DetectionState;
static DetectionState room_detection[MAX_ROOMS] = {0};
static DetectionState* detection_state_get(uint8_t room_id);

// Frame functions
static void process_frame(uint8_t room_id, const uint8_t frame[CAM_RESOLUTION]);
static bool frame_has_line(const uint8_t frame[CAM_RESOLUTION], uint8_t required_count, uint8_t threshold);
static bool row_has_n(const uint8_t frame[CAM_RESOLUTION], uint8_t row, uint8_t required_count, uint8_t threshold);
static bool column_has_n(const uint8_t frame[CAM_RESOLUTION], uint8_t col, uint8_t required_count, uint8_t threshold);
static bool frame_has_occupancy(const uint8_t frame[CAM_RESOLUTION], uint8_t threshold);
static bool pixel_is_occupied(const uint8_t frame[CAM_RESOLUTION], uint8_t row, uint8_t column);
static uint8_t pixel_get(const uint8_t frame[CAM_RESOLUTION], uint8_t row, uint8_t column);

// General Helper
static bool has_consecutive_n(const uint8_t* values, uint8_t length, uint8_t required_count, uint8_t threshold);
static bool update_brightness(uint8_t room_id, float brightness);
static bool turn_off_lights(uint8_t room_id);
static bool turn_on_lights(uint8_t room_id);

void mobile_app_handle(const QueueMessage *msg) {
    switch (msg->app.action)
    {
        case APP_STATUS: {
            if (msg->app.payload.status.connected_to_broker == true) {
                char json_buf[256];
                
                if(!serialize_message(msg, json_buf, sizeof(json_buf))) {
                    ESP_LOGE(TAG, "Serialization Error");
                    return;
                }

                int msg_id = mqtt_transport_publish("/als/status", json_buf); 
                ESP_LOGD(TAG, "Published status message sent, msg_id=%d", msg_id);
            }
            break;
        }
        case APP_GET_MAIN_STATE: {
            char json_buf[256];
            QueueMessage msg_to_app = {0};
            RoomState state_to_send[ROOMS_TO_SEND] = {0};
            for (size_t i = 1; i <= ROOMS_TO_SEND; ++i) { // room_id 1-2
                bool ok = get_room_state(i, &state_to_send[i - 1]);
                if (!ok) {
                    ESP_LOGE(TAG, "Failed to get room state");
                    return;
                }
                log_room_state(&state_to_send[i - 1]);
            }
            
            msg_to_app.origin = msg->origin;
            msg_to_app.device = msg->device;
            msg_to_app.app.action = msg->app.action;
            for (size_t i = 0; i < ROOMS_TO_SEND; ++i) { 
                msg_to_app.app.payload.get_main_state_res.room_state[i] = state_to_send[i];
            }

            if (!serialize_message(&msg_to_app, json_buf, sizeof(json_buf))) {
                ESP_LOGE(TAG, "Serialization Error");
                return;
            }

            int msg_id = mqtt_transport_publish("/als/app/state", json_buf); 
            ESP_LOGD(TAG, "Published status message sent, msg_id=%d", msg_id);
            break;
        }
        case APP_SEND_FRAME:
            process_frame(msg->app.payload.send_frame.room_id, 
                          msg->app.payload.send_frame.pixel_data);
            break;
        default:
            ESP_LOGE(TAG, "Unknown App Action %u", msg->app.action);
            break;
    }
}

static bool get_room_state(uint8_t room_id, RoomState *out) {
    if (out == NULL) return false;
    out->room_id = room_id;
    out->base_color = light_base_color_get(room_id);
    if(!adaptive_lighting_mode_room_state_get(room_id, &out->alm_room_state)) return false;
    return true;
}

static void log_room_state(const RoomState *state) {
    if (state == NULL) return;
    ESP_LOGI(TAG, "Room %u State", state->room_id);
    ESP_LOGI(TAG, "  Base color R: %u G: %u B: %u",
        state->base_color.r, 
        state->base_color.g, 
        state->base_color.b);
    ESP_LOGI(TAG, "  Enabled: %u", state->alm_room_state.alm_enabled);
    if (state->alm_room_state.alm_enabled) {
        ESP_LOGI(TAG, "  Wake Time: %s", state->alm_room_state.wake_time);
        ESP_LOGI(TAG, "  Sleep Time: %s", state->alm_room_state.sleep_time);
    }
}

static DetectionState* detection_state_get(uint8_t room_id) {
    return &room_detection[room_id];
}

static void process_frame(uint8_t room_id, const uint8_t frame[CAM_RESOLUTION]) {
    const uint8_t REQUIRED_PIXELS = 3;
    const uint8_t THRESHOLD = 2;
    const uint8_t LAYING_ON_FRAMES = 5;
    const uint8_t LAYING_OFF_FRAMES = 5;
    const uint8_t PRESENCE_OFF_FRAMES = 50; 
    
    // RIGHT NOW WE DEEM AS OCCUPIED WHEN ANY 2. SHOULD THIS BE A 1
    // bool frame_occupied = frame_has_occupancy(frame, THRESHOLD); 
    
    bool frame_has_laying = frame_has_line(frame, REQUIRED_PIXELS, THRESHOLD);

    DetectionState *state = detection_state_get(room_id);
    
    // if (frame_occupied)  {
    //     state->presence_misses = 0;
    // } else {
    //     state->presence_misses++;
    // }

    if (frame_has_laying) {
        state->laying_hits++;
        state->laying_misses = 0;
    } else {
        state->laying_hits = 0;
        state->laying_misses++;
    }
   
    // Prensence off
    // if (state->any_presence && state->presence_misses >= PRESENCE_OFF_FRAMES) {
    //     state->any_presence = false;
    //     ESP_LOGI(TAG, "Room %u: No presence detected. Turning off lights...", room_id);
    //     if (!turn_off_lights(room_id)) {
    //         ESP_LOGE(TAG, "Failed to send message to queue");
    //     }
    // }

    // // Prensence on 
    // if (!state->any_presence && frame_occupied) {
    //     state->any_presence = true;
    //     ESP_LOGI(TAG, "Room %u: Presence detected. Turning on lights...", room_id);
    //     if (!turn_on_lights(room_id)) {
    //         ESP_LOGE(TAG, "Failed to send message to queue");
    //     }
    // }
    

    // Laying on
    if (!state->laying_detected && state->laying_hits >= LAYING_ON_FRAMES) {
        state->laying_detected = true;
        ESP_LOGI(TAG, "Room %u: Laying down detected. Dimming Lights...", room_id);
        if (!update_brightness(room_id, LAYING_BRIGHTNESS)) {
            ESP_LOGE(TAG, "Failed to send message to queue");
        }
    }
    
    // Laying off
    if (state->laying_detected && state->laying_misses >= LAYING_OFF_FRAMES) {
        state->laying_detected = false;
        ESP_LOGI(TAG, "Room %u: Laying down detection cleared. Restoring Lights...", room_id);
        if (!update_brightness(room_id, 1)) {
            ESP_LOGE(TAG, "Failed to send message to queue");
        }
    }
}

static bool frame_has_line(
    const uint8_t frame[CAM_RESOLUTION], 
    uint8_t required_count, 
    uint8_t threshold
) {
    for (uint8_t row = 0; row < FRAME_SIDE_LENGTH; row++) {
        if (row_has_n(frame, row, required_count, threshold))
            return true;
    }
    
    for (uint8_t col = 0; col < FRAME_SIDE_LENGTH; col++) {
        if (column_has_n(frame, col, required_count, threshold))
            return true;
    }
    return false;
}

static bool row_has_n(
    const uint8_t frame[CAM_RESOLUTION], 
    uint8_t row, 
    uint8_t required_count, 
    uint8_t threshold
) {
    uint8_t temp[FRAME_SIDE_LENGTH];
    
    for (uint8_t col = 0; col < FRAME_SIDE_LENGTH; col++) {
        temp[col] = pixel_get(frame, row, col);
    }

    return has_consecutive_n(temp, FRAME_SIDE_LENGTH, required_count, threshold);
}

static bool column_has_n(
    const uint8_t frame[CAM_RESOLUTION], 
    uint8_t col, 
    uint8_t required_count, 
    uint8_t threshold
) {
    uint8_t temp[FRAME_SIDE_LENGTH];
    
    for (uint8_t row = 0; row < FRAME_SIDE_LENGTH; row++) {
        temp[row] = pixel_get(frame, row, col);
    }

    return has_consecutive_n(temp, FRAME_SIDE_LENGTH, required_count, threshold);
}

static bool frame_has_occupancy(const uint8_t frame[CAM_RESOLUTION], uint8_t threshold) {
    for (size_t i = 0; i < CAM_RESOLUTION; ++i) {
        if (frame[i] >= threshold) return true;
    }
    return false;
}

static bool pixel_is_occupied(const uint8_t frame[CAM_RESOLUTION], uint8_t row, uint8_t column) {
    return (pixel_get(frame, row, column) == 2);
}

static uint8_t pixel_get(const uint8_t frame[CAM_RESOLUTION], uint8_t row, uint8_t column) {
    if (frame == NULL) {
        ESP_LOGE(TAG, "Invalid Frame");
        return 0;
    }
    if (row >= 8 || column >= 8) {
        ESP_LOGW(TAG, "Invalid Bounds");
        return 0;
    }
    uint8_t pixel_location = (row * 8) + column;
    
    return frame[pixel_location];
}

static bool has_consecutive_n(
    const uint8_t* values, 
    uint8_t length, 
    uint8_t required_count, 
    uint8_t threshold
) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < length; i++) {
        if (values[i] >= threshold) {
            count++;
            if (count >= required_count) return true;
        } else {
            count = 0;
        }
    }
    return false;
}


static bool update_brightness(uint8_t room_id, float brightness) {
    QueueMessage light_msg  = {
        .origin = ORIGIN_MAIN,
        .device = DEVICE_LIGHT,
        .light = {
            .action = LIGHT_UPDATE_STATE,
            .payload.state_update = {
                .room_id = room_id,
                .has_brightness = true,
                .room_brightness = brightness 
            }
        }
    };

    return (message_router_push_local(&light_msg) == pdPASS);
}

static bool turn_off_lights(uint8_t room_id) {
    QueueMessage light_msg  = {
        .origin = ORIGIN_MAIN,
        .device = DEVICE_LIGHT,
        .light = {
            .action = LIGHT_UPDATE_STATE,
            .payload.state_update = {
                .room_id = room_id,
                .has_enabled = true,
                .enabled = false 
            }
        }
    };

    return (message_router_push_local(&light_msg) == pdPASS);
}

static bool turn_on_lights(uint8_t room_id) {
    QueueMessage light_msg  = {
        .origin = ORIGIN_MAIN,
        .device = DEVICE_LIGHT,
        .light = {
            .action = LIGHT_UPDATE_STATE,
            .payload.state_update = {
                .room_id = room_id,
                .has_enabled = true,
                .enabled = true
            }
        }
    };

    return (message_router_push_local(&light_msg) == pdPASS);
}