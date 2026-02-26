#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "protocol.h"
#include "cJSON.h"

static const char *TAG = "PROTOCOL";
/*
 * Parsers
*/
static bool parse_app_message(cJSON *root, QueueMessage *out);
// Main
static bool parse_main_message(cJSON *root, QueueMessage *out);
static bool parse_main_heartbeat_update(cJSON *root, QueueMessage *out);
static bool parse_main_occupancy_update(cJSON *root, QueueMessage *out);
// Light
static bool parse_light_message(cJSON *root, QueueMessage *out);
static bool parse_light_set_rgb(cJSON *root, QueueMessage *out);
static bool parse_light_toggle_adaptive_lighting_mode(cJSON *root, QueueMessage *out);

/*
 * Serializers
*/
static bool serialize_app(cJSON* root, const QueueMessage *msg);
static bool serialize_light(cJSON* root, const QueueMessage *msg);

/*
 * Getters (refs)
*/
static bool get_string_ref(cJSON *obj, const char *key, const char** out);

/*
 * Getters (copies)
*/
static bool get_u8(cJSON *obj, const char* key, uint8_t *out);
static bool get_bool(cJSON *obj, const char* key, bool *out);
static bool get_time_hhmm(cJSON *obj, const char* key, char out[6]);


bool parse_broker_message(const char* json, QueueMessage *out) {
    if (!out) return false;

    cJSON *root = cJSON_Parse(json);
   
    const char *device_from_message, *origin;

    bool ok = false;

    if (!root) goto fail;

    if (!get_string_ref(root, "origin", &origin)) goto fail;
    if (!get_string_ref(root, "device", &device_from_message)) goto fail;
    
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


static bool parse_app_message(cJSON *root, QueueMessage *out) {
    // Code here
    bool ok = false;
    return ok;
}

static bool parse_main_message(cJSON *root, QueueMessage *out) {
    const char *action;
    if (!get_string_ref(root, "action", &action)) return false;
    
    out->main.action = main_action_from_string(action);
    
    switch (out->main.action)
    {
        case MAIN_HEARTBEAT_UPDATE:
            return parse_main_heartbeat_update(root, out);
            break;
        case MAIN_OCCUPANCY_UPDATE:
            return parse_main_occupancy_update(root, out);
            break;
        default:
            ESP_LOGI(TAG, "Unknown main action: %s", action);
            return false;
    }
    return true;
}

static bool parse_main_heartbeat_update(cJSON *root, QueueMessage *out) {
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!cJSON_IsObject(payload)) return false;

    return get_bool(payload, "connected_to_broker", 
        &out->main.payload.heartbeat_update.connected_to_broker);
}

static bool parse_main_occupancy_update(cJSON *root, QueueMessage *out) {
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!cJSON_IsObject(payload)) return false;

    return get_bool(payload, "occupied", &out->main.payload.occupancy_update.occupied) &&
           get_u8(payload, "room_id", &out->main.payload.occupancy_update.room_id);
}

static bool parse_light_message(cJSON *root, QueueMessage *out) {
    const char *action;
    if(!get_string_ref(root, "action", &action)) return false;
    
    out->light.action = light_action_from_string(action);
    
    switch (out->light.action)
    {
        case LIGHT_SET_RGB:
            return parse_light_set_rgb(root, out); 
            break;
        case LIGHT_TOGGLE_ADAPTIVE_LIGHTING_MODE:
            return parse_light_toggle_adaptive_lighting_mode(root, out); 
            break;
        default:
            ESP_LOGI(TAG, "Unknown light action: %s", action);
            return false;
            break;
    }
}

static bool parse_light_set_rgb(cJSON *root, QueueMessage *out) {
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!cJSON_IsObject(payload)) return false;

    return get_u8(payload, "r", &out->light.payload.r)
        && get_u8(payload, "g", &out->light.payload.g)
        && get_u8(payload, "b", &out->light.payload.b);
}

static bool parse_light_toggle_adaptive_lighting_mode(cJSON *root, QueueMessage *out) {
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if(!cJSON_IsObject(payload)) return false;
    
    if(!get_bool(payload, 
                 "enabled", 
                 &out->light.payload.toggle_adaptive_lighting_mode.enabled) ||
       !get_u8(payload,
                 "room_id",
                 &out->light.payload.toggle_adaptive_lighting_mode.room_id))
        return false;
    
    if (!out->light.payload.toggle_adaptive_lighting_mode.enabled) {
        return true;
    }

    return get_time_hhmm(payload,
                         "wake_time",
                         out->light.payload.toggle_adaptive_lighting_mode.wake_time)
        && get_time_hhmm(payload, 
                         "sleep_time",   
                         out->light.payload.toggle_adaptive_lighting_mode.sleep_time);
}

bool serialize_message(const QueueMessage *msg, char* out, size_t out_len) {
    if (!msg || !out || out_len == 0) return false;

    cJSON *root = cJSON_CreateObject();
    
    if (!cJSON_AddStringToObject(root, "origin", 
            origin_to_string(msg->origin)) ||
        !cJSON_AddStringToObject(root, "device", 
            device_to_string(msg->device))) {
        cJSON_Delete(root);
        return false;
    }
    
    bool ok = false;
    
    switch (msg->device)
    {
        case DEVICE_MAIN:
            /* code */
            break;
        case DEVICE_APP:
            ok = serialize_app(root, msg);
            break;
        case DEVICE_LIGHT:
            ok = serialize_light(root, msg);
            break;
        case DEVICE_OCC_SENSOR:
            break;
        case DEVICE_UNKNOWN:
            cJSON_Delete(root);
            return false;
        default:
            cJSON_Delete(root);
            return false;
    }
    
    if (!ok) {
       cJSON_Delete(root);
       return false; 
    }
    
    char *tmp = cJSON_PrintUnformatted(root);
    if (!tmp) {
       cJSON_Delete(root);
       return false;
    }
    
    strncpy(out, tmp, out_len);
    out[out_len - 1] = '\0';
    
    free(tmp);
    cJSON_Delete(root);
    return true;
}

static bool serialize_app(cJSON* root, const QueueMessage *msg) {
    
    if (!root || !msg) return false;
    
    const char *action_str = app_action_to_string(msg->app.action);
    
    if (!action_str) return false;
    
    if (!cJSON_AddStringToObject(root, "action", action_str))
        return false;
    
    cJSON *payload = cJSON_CreateObject();
    if (!payload) return false;
    
    bool ok = false;
    
    switch (msg->app.action)
    {
    case APP_STATUS:
        ok = cJSON_AddBoolToObject(payload, "connected_to_broker",
                                   msg->app.payload.connected_to_broker);
        break;
    default:
        cJSON_Delete(payload);
        return false;
    }
    
    if (!ok) {
        cJSON_Delete(payload);
        return false;
    }
    
    cJSON_AddItemToObject(root, "payload", payload);

    return true;
}

static bool serialize_light(cJSON* root, const QueueMessage *msg) {
    if (!root || !msg) return false;
    
    const char *action_str = light_action_to_string(msg->light.action);
    
    if (!action_str) return false;
    
    if (!cJSON_AddStringToObject(root, "action", action_str))
        return false;
    
    cJSON *payload = cJSON_CreateObject();
    if (!payload) return false;
    
    bool ok = false;

    switch (msg->light.action) 
    {
        case LIGHT_SET_RGB:
            ok = 
                cJSON_AddNumberToObject(payload, "r", msg->light.payload.r) &&
                cJSON_AddNumberToObject(payload, "g", msg->light.payload.g) &&
                cJSON_AddNumberToObject(payload, "b", msg->light.payload.b);
            break;
        case LIGHT_TOGGLE_ADAPTIVE_LIGHTING_MODE:
            // Does data even need to go to the lights?
            // Maybe just the initial value of the "adaptive lighting mode" 
            // Should the "toggle adaptive lighting mode" also send the current time as a command too?
            ok = true;
            break;
        default:
            cJSON_Delete(payload);
            return false;
    }
    
    if (!ok) {
       cJSON_Delete(payload); 
       return false;
    }
    
    cJSON_AddItemToObject(root, "payload", payload);

    return true;
}

MessageOrigin origin_from_string(const char *s) {
    if (!strcmp(s, "MAIN")) return ORIGIN_MAIN;
    if (!strcmp(s, "APP")) return ORIGIN_APP;
    if (!strcmp(s, "OCC")) return ORIGIN_OCC_SENSOR;
    return ORIGIN_UKNOWN;
}

DeviceType device_from_string(const char *s) {
    if (!strcmp(s, "MAIN")) return DEVICE_MAIN;
    if (!strcmp(s, "APP")) return DEVICE_APP;
    if (!strcmp(s, "LIGHT")) return DEVICE_LIGHT;
    if (!strcmp(s, "OCC")) return DEVICE_OCC_SENSOR;
    return DEVICE_UNKNOWN;
}

MainAction main_action_from_string(const char *s) {
    if (!strcmp(s, "HEARTBEAT_UPDATE")) return MAIN_HEARTBEAT_UPDATE;
    if (!strcmp(s, "OCC_UPDATE")) return MAIN_OCCUPANCY_UPDATE;
    return MAIN_UNKNOWN;
}

LightAction light_action_from_string(const char *s) {
    if (!strcmp(s, "SET_RGB")) return LIGHT_SET_RGB;
    if (!strcmp(s, "TOGGLE_ADAPTIVE_LIGHTING_MODE")) return LIGHT_TOGGLE_ADAPTIVE_LIGHTING_MODE;
    return LIGHT_UNKNOWN;
}

const char* origin_to_string(MessageOrigin origin) {
    switch (origin)
    {
        case ORIGIN_APP:
            return "APP";
        case ORIGIN_MAIN:
            return "MAIN";
        case ORIGIN_OCC_SENSOR:
            return "OCC";
        case ORIGIN_UKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

const char* device_to_string(DeviceType device) {
    switch (device)
    {
        case DEVICE_MAIN:
            return "MAIN";
        case DEVICE_APP:
            return "APP";
        case DEVICE_LIGHT:
            return "LIGHT";
        case DEVICE_OCC_SENSOR:
            return "OCC";
        case DEVICE_UNKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

const char* app_action_to_string(AppAction app_action) {
    switch (app_action)
    {
        case APP_STATUS:
            return "STATUS";
        default:
            return NULL;
    }
}

const char* light_action_to_string(LightAction light_action) {
    switch (light_action)
    {
        case LIGHT_SET_RGB:
            return "SET_RGB";
        case LIGHT_TOGGLE_ADAPTIVE_LIGHTING_MODE:
            return "TOGGLE_ADAPTIVE_LIGHTING_MODE";
        default:
            return NULL;
    }
}

static bool get_string_ref(cJSON *obj, const char *key, const char** out) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsString(item)) return false;
    *out = item->valuestring;
    return true;
}

static bool get_u8(cJSON *obj, const char* key, uint8_t *out) {
   cJSON *item = cJSON_GetObjectItem(obj, key); 
   if (!cJSON_IsNumber(item)) return false;

   int v = item->valuedouble; // Maybe swap back to double?
   if (v < 0 || v > 255) return false;
   
   *out = (uint8_t)v;
   return true;
}

static bool get_bool(cJSON *obj, const char* key, bool *out) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsBool(item)) return false;
    
    bool b = item->valueint;
    *out = b;
    return true;
}

static bool get_time_hhmm(cJSON *obj, const char* key, char out[6]) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsString(item)) return false;
    const char *s = item->valuestring;
    if (strlen(s) != 5) return false;
    if (s[2] != ':') return false;

    if (!isdigit((unsigned char)s[0]) ||
        !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[3]) ||
        !isdigit((unsigned char)s[4]))
        return false;

    memcpy(out, s, 6);
    return true;
}
