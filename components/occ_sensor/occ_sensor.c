#include <stdio.h>
#include "occ_sensor.h"
#include "transport_mqtt.h"

static void set_room_off_delay(uint8_t room_id, uint16_t off_delay);

static const char *TAG = "OCC";

static uint16_t room_off_delay[MAX_ROOMS];


void occ_handle(const QueueMessage *msg) {
    switch (msg->occ.action)
    {
        case OCC_CONFIG_DELAY: {
                                   
            char topic_buffer[64];
            uint8_t room_id = msg->occ.payload.config_delay.room_id;

            snprintf(topic_buffer, sizeof(topic_buffer), 
                "/als/occ/room/%u", room_id);

            char json_buf[256];
            
            if (!serialize_message(msg, json_buf, sizeof(json_buf))) {
                ESP_LOGE(TAG, "Serialization Error");
                return;
            }

            set_room_off_delay(room_id,
                                msg->occ.payload.config_delay.off_delay);
            
            int msg_id = mqtt_transport_publish(topic_buffer, json_buf);
            ESP_LOGD(TAG, "Published status message sent, msg_id=%d", msg_id);
            break;
        }
        
        default:
            break;
    }
}

static void set_room_off_delay(uint8_t room_id, uint16_t off_delay) {
    room_off_delay[room_id] = off_delay;
}