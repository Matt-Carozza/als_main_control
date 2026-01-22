#include <stdio.h>
#include "protocol.h"
#include "cJSON.h"

static const char *TAG = "PROTOCOL";

static bool get_string(cJSON *obj, const char *key, const char** out);
static bool get_u8(cJSON *obj, const char* key, uint8_t *out);

bool parse_broker_message(const char* json, QueueMessage *out) {
    if (!out) return false;

    cJSON *root = cJSON_Parse(json);
   
    const char *device_from_message, *origin;

    bool ok = false;

    if (!root) goto fail;

    if (!get_string(root, "origin", &origin)) goto fail;
    if (!get_string(root, "device", &device_from_message)) goto fail;
    
    out->origin = origin_from_string(origin);
    out->device = device_from_string(device_from_message);
    
    switch (out->device)
    {
        case DEVICE_APP:
            /* code */
            break;
        case DEVICE_LIGHT:
            ok = parse_light_message(root, out);
            /* code */
            break;
        case DEVICE_OCC_SENSOR:
            /* code */
            break;
        case DEVICE_UNKNOWN:
            ESP_LOGI(TAG, "Unknown Device Type: %s", device_from_message);
            break;
        default:
            break;
    }
    
fail:
    if (root) cJSON_Delete(root);
    return ok;
}

MessageOrigin origin_from_string(const char *s) {
    if (!strcmp(s, "MAIN")) return ORIGIN_MAIN;
    if (!strcmp(s, "APP")) return ORIGIN_APP;
    return ORIGIN_UKNOWN;
}

DeviceType device_from_string(const char *s) {
    if (!strcmp(s, "APP")) return DEVICE_APP;
    if (!strcmp(s, "LIGHT")) return DEVICE_LIGHT;
    return DEVICE_UNKNOWN;
}

bool parse_app_message(cJSON *root, QueueMessage *out) {
    // Code here
    bool ok = false;
    return ok;
}

bool parse_light_message(cJSON *root, QueueMessage *out) {
    const char *action;
    if(!get_string(root, "action", &action)) return false;
    
    out->light.action = light_action_from_string(action);
    
    switch (out->light.action)
    {
        case LIGHT_SET:
            /* code */
            return parse_light_set(root, out); 
            break;
        case LIGHT_UNKNOWN:
            ESP_LOGI(TAG, "Unknown light action: %s", action);
            return false;
            break;
        default:
            ESP_LOGI(TAG, "Unknown light action: %s", action);
            return false;
            break;
    }
}

LightAction light_action_from_string(const char *s) {
    if (!strcmp(s, "SET")) return LIGHT_SET;
    return LIGHT_UNKNOWN;
}

bool parse_light_set(cJSON *root, QueueMessage *out) {
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if(!cJSON_IsObject(payload)) return false;

    return get_u8(payload, "r", &out->light.payload.r)
        && get_u8(payload, "g", &out->light.payload.g)
        && get_u8(payload, "b", &out->light.payload.b);
        
    return true;
}

static bool get_string(cJSON *obj, const char *key, const char** out) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsString(item)) return false;
    *out = item->valuestring;
    return true;
}

static bool get_u8(cJSON *obj, const char* key, uint8_t *out) {
   cJSON *item = cJSON_GetObjectItem(obj, key); 
   if (!cJSON_IsNumber(item)) return false;

   double v = item->valuedouble;
   if (v < 0 || v > 255) return false;
   
   *out = (uint8_t)v;
   return true;
}