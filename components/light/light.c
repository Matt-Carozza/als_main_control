#include <stdio.h>
#include "light.h"
#include "transport_mqtt.h"

static const char* TAG = "LIGHT";

void light_handle(const QueueMessage *msg) {
    switch (msg->light.action)
    {
        case LIGHT_SET_RGB:        
            char topic_buffer[64];
            snprintf(topic_buffer, sizeof(topic_buffer), 
                "/als/light/room/%u", 
                msg->light.payload.set_rgb.room_id);

            char json_buf[256];
            if(!serialize_message(msg, json_buf, sizeof(json_buf))) {
                ESP_LOGE(TAG, "Serialization Error");
                return;
            }

            int msg_id = mqtt_transport_publish(topic_buffer, json_buf); 
            ESP_LOGI(TAG, "Published status message sent, msg_id=%d", msg_id);
            break;
            
        default:
            break;
    }
}
