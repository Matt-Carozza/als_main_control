#pragma once 

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "cJSON.h"

typedef enum {
    ORIGIN_MAIN,
    ORIGIN_APP,
    ORIGIN_OCC_SENSOR,
    ORIGIN_DAY_SENSOR,
    ORIGIN_UKNOWN
} MessageOrigin;

typedef enum {
    DEVICE_MAIN,
    DEVICE_APP,
    DEVICE_LIGHT,
    DEVICE_OCC_SENSOR,
    DEVICE_UNKNOWN 
} DeviceType;

typedef enum {
    APP_STATUS,
} AppAction;

typedef enum {
    // OCC
    MAIN_HEARTBEAT_UPDATE,
    MAIN_OCCUPANCY_UPDATE,
    MAIN_TOGGLE_ADAPTIVE_LIGHTING_MODE,
    MAIN_DAY_UPDATE,
    MAIN_UNKNOWN
} MainAction;

typedef struct {
    union {
        struct {
            bool connected_to_broker;
        } heartbeat_update;
        struct {
            bool occupied;
            uint8_t room_id; 
        } occupancy_update;
        struct {
            uint8_t room_id; 
            float voltage;
        } day_update;
        struct { 
            uint8_t room_id;
            bool enabled;
            char wake_time[6]; 
            char sleep_time[6]; 
        } toggle_adaptive_lighting_mode;
    };
} MainPayload;

typedef enum {
    LIGHT_SET_RGB,
    LIGHT_UNKNOWN 
} LightAction;

typedef struct {
    union {
        struct { 
            uint8_t room_id, r, g, b; 
        } set_rgb;
    };
} LightPayload;

typedef struct {
    bool connected_to_broker;
} AppPayload;

typedef struct {
    MessageOrigin origin;
    DeviceType device;
    
    union {
        struct {
            MainAction action;
            MainPayload payload;
        } main;
       struct {
           AppAction action;
           AppPayload payload;
       } app;
       struct {
           LightAction action;
           LightPayload payload;
       } light;
    };
} QueueMessage;


bool parse_broker_message(const char* json, QueueMessage *msg);
bool serialize_message(const QueueMessage *msg, char* out, size_t out_len);

/*
    Turns keys (string) found within JSON objects into corresponding enums
*/

MessageOrigin origin_from_string(const char *s);
DeviceType device_from_string(const char *s);
MainAction main_action_from_string(const char *s);
LightAction light_action_from_string(const char *s);

/*
    Turns enumerations found within QueueMessage struct into corresponding strings
*/

const char* origin_to_string(MessageOrigin origin);
const char* device_to_string(DeviceType device);
const char* app_action_to_string(AppAction app_action);
const char* light_action_to_string(LightAction app_action);