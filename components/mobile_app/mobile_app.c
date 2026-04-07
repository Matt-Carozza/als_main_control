#include <stdio.h>
#include "mobile_app.h"
#include "transport_mqtt.h"
#include "adaptive_lighting_mode.h"

static const char* TAG = "MOBILE_APP";

static bool get_room_state(uint8_t room_id, RoomState *out);
static void log_room_state(const RoomState *state);

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

            if(!serialize_message(&msg_to_app, json_buf, sizeof(json_buf))) {
                ESP_LOGE(TAG, "Serialization Error");
                return;
            }

            int msg_id = mqtt_transport_publish("/als/app/state", json_buf); 
            ESP_LOGD(TAG, "Published status message sent, msg_id=%d", msg_id);
            break;
        }
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