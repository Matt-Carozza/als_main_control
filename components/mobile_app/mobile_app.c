#include <stdio.h>
#include "mobile_app.h"
#include "transport_mqtt.h"

static const char* TAG = "MOBILE_APP";

void mobile_app_handle(const QueueMessage *msg) {
    switch (msg->app.action)
    {
        case APP_STATUS:        
            if (msg->app.payload.connected_to_broker == true) {
                char json_buf[256];
                
                if(!serialize_message(msg, json_buf, sizeof(json_buf))) {
                    ESP_LOGE(TAG, "Serialization Error");
                    return;
                }

                int msg_id = mqtt_transport_publish("/als/status", json_buf); 
                ESP_LOGI(TAG, "Published status message sent, msg_id=%d", msg_id);
            }
            break;
        
        default:
            break;
    }

    
}
