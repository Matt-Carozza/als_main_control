#pragma once 

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "cJSON.h"

#define MAX_ROOMS 256U
#define ROOMS_TO_SEND 2U // 2 Is needed for Demo Day


#define CAM_RESOLUTION 64U
#define PIXEL_MAX_BIN_SIZE 2
#define FRAME_SIDE_LENGTH 8

typedef enum {
    ORIGIN_MAIN,
    ORIGIN_APP,
    ORIGIN_OCC_SENSOR,
    ORIGIN_DAY_SENSOR,
    ORIGIN_CAMERA,
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
    APP_GET_MAIN_STATE,
    APP_SEND_FRAME,
    APP_UNKNOWN
} AppAction;

typedef enum {
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
            uint32_t voltage;
        } day_update;
        struct { 
            uint8_t room_id;
            bool enabled;
            char wake_time[6]; 
            char sleep_time[6];
            char current_time[6]; 
        } toggle_adaptive_lighting_mode;
    };
} MainPayload;

typedef enum {
    LIGHT_SET_RGB,
    LIGHT_UPDATE_STATE,
    LIGHT_UNKNOWN 
} LightAction;

typedef struct {
    union {
        struct { 
            uint8_t room_id, r, g, b; 
        } set_rgb;
        struct {
            uint8_t room_id;
            bool has_enabled;
            bool enabled;
            bool has_brightness;
            float room_brightness;
        } state_update;
    };
} LightPayload;

typedef enum {
    OCC_CONFIG_DELAY,
    OCC_UNKNOWN
} OccAction;

typedef struct {
    union {
        struct {
            uint8_t room_id;
            uint16_t off_delay;
        } config_delay;
    };
} OccPayload;

typedef struct {
    uint8_t r, g, b;
} RGB;
typedef struct {
    char wake_time[6];
    char sleep_time[6];
    bool alm_enabled;
} AlmRoom;

typedef struct {
    AlmRoom alm_room_state;
    RGB base_color;
    uint8_t room_id;
} RoomState;

typedef struct {
    union {
        struct {
            bool connected_to_broker;
        } status;

        struct {
            // No fields needed
        } get_main_state_req;
        struct {
            RoomState room_state[ROOMS_TO_SEND];
        } get_main_state_res;
        struct {
            uint8_t pixel_data[CAM_RESOLUTION];
            uint8_t room_id;
        } send_frame;
    };
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
       struct {
            OccAction action;
            OccPayload payload;
       } occ;
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
OccAction occ_action_from_string(const char *s);
AppAction app_action_from_string(const char *s);

/*
    Turns enumerations found within QueueMessage struct into corresponding strings
*/

const char* origin_to_string(MessageOrigin origin);
const char* device_to_string(DeviceType device);
const char* app_action_to_string(AppAction app_action);
const char* light_action_to_string(LightAction app_action);
const char* occ_action_to_string(OccAction occ_action);