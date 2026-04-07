#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"

#include "message_router.h"
#include "transport_mqtt.h"
#include "protocol.h"

#include "main_control.h"
#include "mobile_app.h"
#include "light.h"
#include "occ_sensor.h"
#include "adaptive_lighting_mode.h"


void queue_task(void *pvParameters);
void status_task(void *pvParameters);

static const char *TAG = "mqtt_example";



void queue_task(void *pvParameters) {
    QueueMessage msg;
    while (1) {
        if (message_router_receive(&msg) == pdPASS) {
            switch (msg.device) {
                case DEVICE_MAIN:
                    main_control_handle(&msg);
                    break;
                case DEVICE_APP:
                    mobile_app_handle(&msg);
                    break;
                case DEVICE_LIGHT:
                    light_handle(&msg);
                    break;
                case DEVICE_OCC_SENSOR:
                    occ_handle(&msg);
                    break;
                case DEVICE_UNKNOWN:
                    ESP_LOGE(TAG, "ERROR During Queue: Device Uknown");
                    break;
            }
        }
    } 
}

void status_task(void *pvParameters) {
    QueueMessage msg = { 
        .origin = ORIGIN_MAIN,
        .device = DEVICE_APP,
        .app = {
            .action = APP_STATUS
        }
    };

    while (1) {
        msg.app.payload.status.connected_to_broker = mqtt_transport_is_connected();

        if (message_router_push_local(&msg) != pdPASS) { 
            ESP_LOGE("STATUS_TASK", "Failed to send message to queue");
        } 

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    
    message_router_init();
    adaptive_lighting_mode_init();

    xTaskCreate(queue_task, "queue_task", 4096, NULL, 5, NULL);
    xTaskCreate(status_task, "status_task", 4096, NULL, 3, NULL);
    
    mqtt_transport_start();
}
