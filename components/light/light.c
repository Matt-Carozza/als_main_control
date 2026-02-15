#include <stdio.h>
#include "light.h"
#include "transport_mqtt.h"

static const char* TAG = "LIGHT";

void light_handle(const QueueMessage *msg) {
    switch (msg->light.action)
    {
        case LIGHT_SET_RGB:        
            char json_buf[256];
            
            if(!serialize_message(msg, json_buf, sizeof(json_buf))) {
                ESP_LOGE(TAG, "Serialization Error");
                return;
            }

            int msg_id = mqtt_transport_publish("/als/light", json_buf); 
            ESP_LOGI(TAG, "Published status message sent, msg_id=%d", msg_id);
            break;
            
        case LIGHT_TOGGLE_ADAPTIVE_LIGHTING_MODE:
            break;
        default:
            break;
    }
}
