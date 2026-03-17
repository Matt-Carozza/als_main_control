#include <stdio.h>
#include "light.h"
#include "transport_mqtt.h"
#include "protocol.h"

static RGB get_room_output_color(uint8_t room_id);
static void set_room_base_color(uint8_t room_id, RGB rgb);
static void set_room_enabled(uint8_t room_id, bool enabled);
static void set_room_brightness(uint8_t room_id, float brightness);
static void publish_rgb(MessageOrigin origin, DeviceType device, uint8_t room_id, RGB color_output);
static void init_room_if_needed(uint8_t room_id);
static void log_room_state(uint8_t room_id, RGB output_color);

static const char* TAG = "LIGHT";


static RGB room_base_color[MAX_ROOMS];
static float room_brightness[MAX_ROOMS];
static bool room_enabled[MAX_ROOMS];
static bool room_initialized[MAX_ROOMS];

void light_handle(const QueueMessage *msg) {
    switch (msg->light.action)
    {
        case LIGHT_SET_RGB: { 
            uint8_t room_id = msg->light.payload.set_rgb.room_id;
            
            RGB color = { 
                .r = msg->light.payload.set_rgb.r,
                .g = msg->light.payload.set_rgb.g,
                .b = msg->light.payload.set_rgb.b
            };
            set_room_base_color(room_id, color);

            RGB output_color = get_room_output_color(room_id);

            publish_rgb(msg->origin, msg->device,
                        msg->light.payload.set_rgb.room_id,
                        output_color);

            break;
        }

        // Internal Command
        case LIGHT_UPDATE_STATE: {
            uint8_t room_id = msg->light.payload.state_update.room_id;
            if (msg->light.payload.state_update.has_enabled) 
                set_room_enabled(room_id,
                                msg->light.payload.state_update.enabled);
            if (msg->light.payload.state_update.has_brightness) 
                set_room_brightness(room_id,
                                    msg->light.payload.state_update.room_brightness);
            
            RGB color_output = get_room_output_color(room_id);
            publish_rgb(msg->origin, msg->device, room_id, color_output);
            break;
        }

        default:
            break;
    }
}

static RGB get_room_output_color(uint8_t room_id) {
    init_room_if_needed(room_id);
    if (!room_enabled[room_id]) return (RGB) { .r = 0, .g = 0, .b = 0 };
    return (RGB) {
        .r = (uint8_t)(room_base_color[room_id].r * room_brightness[room_id]),
        .g = (uint8_t)(room_base_color[room_id].g * room_brightness[room_id]),
        .b = (uint8_t)(room_base_color[room_id].b * room_brightness[room_id])
    };
}

static void set_room_base_color(uint8_t room_id, RGB rgb) {
    init_room_if_needed(room_id);
    room_base_color[room_id] = rgb;
}

static void set_room_enabled(uint8_t room_id, bool enabled) {
    init_room_if_needed(room_id);
    room_enabled[room_id] = enabled;
}

static void set_room_brightness(uint8_t room_id, float brightness) {
    init_room_if_needed(room_id);
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;
    room_brightness[room_id] = brightness;
}

static void init_room_if_needed(uint8_t room_id) {
    if (room_initialized[room_id]) return;
    room_base_color[room_id] = (RGB){ .r = 255, .g = 255, .b = 255 };
    room_brightness[room_id] = 1.0f;
    room_enabled[room_id] = true;
    room_initialized[room_id] = true;
}

static void publish_rgb(MessageOrigin origin, DeviceType device, uint8_t room_id, RGB color_output) {
    QueueMessage msg = {
        .origin = origin,
        .device = device,
        .light.action = LIGHT_SET_RGB,
        .light.payload.set_rgb = {
            .room_id = room_id,
            .r = color_output.r,
            .g = color_output.g,
            .b = color_output.b
        }
    };

    char topic_buffer[64];
    snprintf(topic_buffer, sizeof(topic_buffer), 
        "/als/light/room/%u", room_id);

    char json_buf[256];
    if(!serialize_message(&msg, json_buf, sizeof(json_buf))) {
        ESP_LOGE(TAG, "Serialization Error");
        return;
    }

    log_room_state(room_id, color_output);

    int msg_id = mqtt_transport_publish(topic_buffer, json_buf); 
    ESP_LOGD(TAG, "Published status message sent, msg_id=%d", msg_id);
}

static void log_room_state(uint8_t room_id, RGB output_color) {
    ESP_LOGD(TAG, "Room %u state:", room_id);
    ESP_LOGD(TAG, "  Base: R:%u G:%u B:%u",
             room_base_color[room_id].r,
             room_base_color[room_id].g,
             room_base_color[room_id].b);
    ESP_LOGD(TAG, "  Brightness: %.3f", room_brightness[room_id]);
    ESP_LOGD(TAG, "  Enabled: %d", room_enabled[room_id]);
    ESP_LOGD(TAG, "  Output: R:%u G:%u B:%u",
             output_color.r,
             output_color.g,
             output_color.b);
}