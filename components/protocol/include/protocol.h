#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "cJSON.h"

typedef enum {
    ORIGIN_MAIN,
    ORIGIN_APP,
    ORIGIN_UKNOWN
} MessageOrigin;

typedef enum {
    DEVICE_APP,
    DEVICE_LIGHT,
    DEVICE_OCC_SENSOR,
    DEVICE_UNKNOWN 
} DeviceType;

typedef enum {
    APP_STATUS,
    APP_STATE
} AppAction;

typedef enum {
    LIGHT_SET,
    LIGHT_UNKNOWN 
} LightAction;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} LightPayload;

typedef struct {
    bool connected_to_broker;
} AppPayload;

typedef struct {
    MessageOrigin origin;
    DeviceType device;
    
    union {
       struct {
           AppAction action;
           AppPayload payload;
       } app;
       struct {
           LightAction action;
           LightPayload payload;
       } light;
       // Main Control Action / Payload
    };
} QueueMessage;

bool parse_broker_message(const char* json, QueueMessage *msg);
MessageOrigin origin_from_string(const char *s);
DeviceType device_from_string(const char *s);

bool parse_app_message(cJSON *root, QueueMessage *msg);

bool parse_light_message(cJSON *root, QueueMessage *msg);
LightAction light_action_from_string(const char *s);

bool parse_light_set(cJSON *root, QueueMessage *out);
