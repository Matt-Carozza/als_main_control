#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"

#include "message_router.h"
#include "transport_mqtt.h"
#include "protocol.h"

#include "mobile_app.h"
#include "light.h"


// #include "string_type.h" PROB REMOVE

void queue_task(void *pvParameters);
void status_task(void *pvParameters);

static const char *TAG = "mqtt_example";



void queue_task(void *pvParameters) {
    QueueMessage msg;
    while (1) {
        if (message_router_receive(&msg) == pdPASS) {
            switch (msg.device) {
                case DEVICE_APP:
                    mobile_app_handle(&msg);
                    break;
                case DEVICE_LIGHT:
                    light_handle(&msg);
                    break;
                case DEVICE_OCC_SENSOR:
                    break;
                case DEVICE_UNKNOWN:
                    ESP_LOGE(TAG, "ERROR During Queue: Device Uknown");
                    break;
            }
        }
    } 
}

void status_task(void *pvParameters) {
    while (1) {
        QueueMessage msg = { // Should this be scoped outside of the loop
            .origin = ORIGIN_MAIN,
            .device = DEVICE_APP,
            .app = {
                .action = APP_STATUS,
                .payload = {
                    .connected_to_broker = mqtt_transport_is_connected(),
                }
            }
        };

        if (message_router_push_local(&msg) != pdPASS) { 
            ESP_LOGE("STATUS_TASK", "Failed to send message to queue");
        } 
        vTaskDelay(10000 / portTICK_PERIOD_MS);
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

    xTaskCreate(queue_task, "queue_task", 4096, NULL, 5, NULL);
    xTaskCreate(status_task, "status_task", 4096, NULL, 4, NULL);
    
    mqtt_transport_start();
}
