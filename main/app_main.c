// Auth Mode: Open for networks with no passwords, WPA2 PSK with passwords
#define LOCAL_BROKER_URL "mqtt://172.20.10.14:1884"
#define PUBLIC_BROKER_URL "mqtt://test.mosquitto.org:1883"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "protocol.h"

// #include "string_type.h" PROB REMOVE

void queue_task(void *pvParameters);
void status_task(void *pvParameters);

esp_mqtt_client_handle_t client;
QueueHandle_t message_queue;

static const char *TAG = "mqtt_example";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event_info = event_data; // Tells us to treat this void * data as a pointer to an event struct
    esp_mqtt_client_handle_t client = event_info->client;
    
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        
        // Subscribe to command topic (Change name to refelect location)
        esp_mqtt_client_subscribe(client, "/mainControl/commands", 1);
        
        // Start queue task
        message_queue = xQueueCreate(10, sizeof(QueueMessage)); 
        xTaskCreate(&queue_task, "queue_task", 4096, NULL, 4, NULL);
        // Start status task
        xTaskCreate(&status_task, "status_task", 4096, NULL, 5, NULL);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event_info->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event_info->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event_info->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");

        QueueMessage msg;
        bool ok = false;
        ok = parse_broker_message(event_info->data, &msg);
        
        if (!ok) {
            ESP_LOGE(TAG, "Error parsing message");
            break;
        }

        xQueueSend(message_queue, &msg, portMAX_DELAY);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event_info->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event_info->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event_info->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event_info->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event_info->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event_info->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = PUBLIC_BROKER_URL
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void queue_task(void *pvParameters) {
    QueueMessage msg;
    while (1) {
        if (xQueueReceive(message_queue, &(msg), portMAX_DELAY) == pdPASS) {
            switch (msg.device) {
                case DEVICE_APP:
                    switch (msg.app.action) // Wrap this in its own handler
                    {
                        case APP_STATUS:
                            if (client && msg.app.payload.connected_to_broker == true) { // Possibly wrap this ?
                                int msg_id;
                                const char *status_message = "Main Control Alive";
                                msg_id = esp_mqtt_client_publish(client, 
                                    "/mainControl/status", 
                                    status_message, 0, 1, 0);
                                ESP_LOGI(TAG, "Published status message sent, msg_id=%d", msg_id);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                case DEVICE_LIGHT:
                    uint8_t r = msg.light.payload.r;
                    uint8_t g = msg.light.payload.g;
                    uint8_t b = msg.light.payload.b;
                    ESP_LOGI(TAG, "%u %u %u", r, g, b);
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
                    .connected_to_broker = true,
                }
            }
        };

        if (xQueueSend(message_queue, &msg, portMAX_DELAY) != pdPASS) {
           ESP_LOGE("STATUS_TASK", "Failed to send message to queue");
        } 
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
