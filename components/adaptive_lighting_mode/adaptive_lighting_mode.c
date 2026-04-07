#include <stdio.h>
#include <math.h>
#include "adaptive_lighting_mode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "color_temp_to_rgb_lut.h"
#include "message_router.h"

#define DAY_IN_MINUTES 1440U
#define AMPLITUDE ((MAX_COLOR_TEMP - MIN_COLOR_TEMP) / 2.0f)

// ===== State =====
typedef struct {    
    RGB color_before_enable;
    uint16_t wake_minutes;
    uint16_t awake_period;
    bool is_enabled;
} AlmRoomInternal;

typedef struct {
    AlmRoomInternal room_state[MAX_ROOMS];
    uint16_t system_time;
    bool task_running;
} AlmState;

static const char* TAG = "ADAPTIVE LIGHTING MODE";

static SemaphoreHandle_t alm_state_mutex = NULL;
static TaskHandle_t adaptive_lighting_mode_handler;
static AlmState s_state = {0};

// ===== State Transisions =====
static void alm_room_color_before_enable_set_unsafe(AlmState* state, uint8_t room_id, RGB rgb);
static void alm_room_enable_unsafe(AlmState* state, uint8_t room_id);
static void alm_room_disable_unsafe(AlmState* state, uint8_t room_id);
static void alm_room_wake_minutes_set_unsafe(AlmState* state, uint8_t room_id, uint16_t wake_minutes);
static void alm_room_awake_period_set_unsafe(AlmState* state, uint8_t room_id, uint16_t awake_period);
static void alm_system_time_set_unsafe(AlmState* state, uint16_t system_time);
static void alm_system_time_increment_unsafe(AlmState* state);
static void alm_task_set_running_unsafe(AlmState* state);
static void alm_task_set_idle_unsafe(AlmState* state);
// ------ Mutex Safety ------ 
static void alm_room_color_before_enable_set(AlmState* state, uint8_t room_id, RGB rgb);
static void alm_room_enable(AlmState* state, uint8_t room_id);
static void alm_room_disable(AlmState* state, uint8_t room_id);
static void alm_room_wake_minutes_set(AlmState* state, uint8_t room_id, uint16_t wake_minutes);
static void alm_room_awake_period_set(AlmState* state, uint8_t room_id, uint16_t awake_period);
static void alm_system_time_set(AlmState* state, uint16_t system_time);
static void alm_system_time_increment(AlmState* state);
static void alm_task_set_running(AlmState* state);
static void alm_task_set_idle(AlmState* state);

// ===== State Getters =====
// !!!!! Mutex Unsafe !!!!!
static AlmState alm_get_snapshot_unsafe();
static AlmRoomInternal alm_room_get_unsafe(uint8_t room_id);
static RGB alm_room_color_before_enable_get_unsafe(uint8_t room_id);
static bool alm_room_is_enabled_unsafe(uint8_t room_id);
static uint16_t alm_system_time_get_unsafe();
static bool alm_task_is_running_unsafe();
// !!!!!!!!!!!!!!!!!!!!!!!!

// ------ Mutex Safety ------ 
static AlmState alm_get_snapshot();
static AlmRoomInternal alm_room_get(uint8_t room_id);
static RGB alm_room_color_before_enable_get(uint8_t room_id);
static bool alm_room_is_enabled(uint8_t room_id);
static uint16_t alm_system_time_get();
static bool alm_task_is_running();
static uint8_t alm_room_count_get();

// ===== Pure functions =====
static float alm_room_compute_position(const AlmRoomInternal* room, uint16_t time);
static float alm_room_compute_color_temperature(float pos);
static bool hhmm_to_minutes(const char hhmm[6], uint16_t* total_minutes);
static bool minutes_to_hhmm(uint16_t total_minutes, char hhmm[6]); // Might need arg type changed


// ===== Initializers =====
static void alm_task_create();

// ===== Handlers =====
static void adaptive_lighting_mode(void *pvParameters);
static void alm_wake_and_sleep_init(const uint8_t room_id,
    const char wake_time[6], 
    const char sleep_time[6],
    const char current_time[6]);

// ===== Light =====
static void update_light_by_color_temperature(uint8_t room_id, uint16_t color_temperature);
static void update_light_by_rgb(uint8_t room_id, RGB rgb);
static void update_lights_power_off(uint8_t room_id);


void adaptive_lighting_mode_init() {
    if (alm_state_mutex == NULL) {
        alm_state_mutex = xSemaphoreCreateMutex();
    }
}

void adaptive_lighting_mode_enable(const uint8_t room_id, 
    const char wake_time[6], 
    const char sleep_time[6],
    const char current_time[6]) {
    ESP_LOGI(TAG, "ENABLE");

    alm_wake_and_sleep_init(room_id, wake_time, sleep_time, current_time);

    if (alm_room_is_enabled(room_id)) return;

    alm_room_color_before_enable_set(&s_state, room_id, light_base_color_get(room_id));

    if (adaptive_lighting_mode_handler == NULL) {
        ESP_LOGI(TAG, "Creating Handler...");
        alm_task_create();
    }

    if (alm_room_count_get() == 0)  {
        ESP_LOGI(TAG, "Starting ALM Task...");
        alm_task_set_running(&s_state);
    }

    alm_room_enable(&s_state, room_id);
}

void adaptive_lighting_mode_disable(const uint8_t room_id) {
    ESP_LOGI(TAG, "DISABLE ROOM %u", room_id);
    
    if (!alm_room_is_enabled(room_id)) return;
    alm_room_disable(&s_state, room_id);

    if (alm_room_count_get() == 0) {
        ESP_LOGI(TAG, "Disabling ALM Task...");
        alm_task_set_idle(&s_state);
    }

    update_light_by_rgb(room_id, 
                        alm_room_color_before_enable_get(room_id));
}

bool adaptive_lighting_mode_room_state_get(uint8_t room_id, AlmRoom* out) {
    if (out == NULL) return false;

    const AlmRoomInternal room = alm_room_get(room_id);
    out->alm_enabled = room.is_enabled;

    if (!out->alm_enabled) return true; // If false, no wake and sleep time will be input

    if (!minutes_to_hhmm(room.wake_minutes, out->wake_time)) return false;

    uint16_t sleep_total_minutes = (room.wake_minutes + room.awake_period) % DAY_IN_MINUTES;
    if (!minutes_to_hhmm(sleep_total_minutes, out->sleep_time)) return false;

    return true;
}

static void adaptive_lighting_mode(void *pvParameters) {
    const TickType_t x_frequency = pdMS_TO_TICKS(60000);
    TickType_t x_last_wake_time = xTaskGetTickCount();
    bool was_running = false;

    static uint16_t room_last_color_temp_sent[MAX_ROOMS] = {0};
    static bool room_light_status[MAX_ROOMS];
    for (size_t i = 0; i < MAX_ROOMS; i++) {
        room_light_status[i] = true; 
    }

    while (1) {
        const uint16_t current_time = alm_system_time_get();
        const bool task_running = alm_task_is_running();
        
        if (task_running && !was_running) {
            x_last_wake_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "Resetting timer");
        }
        was_running = task_running;

        if (!task_running) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        alm_system_time_increment(&s_state);

        for (size_t room_id = 0; room_id < MAX_ROOMS; room_id++) {
            const AlmRoomInternal room = alm_room_get(room_id);
            if (!room.is_enabled) continue;

            float current_pos = alm_room_compute_position(&room, current_time);
            ESP_LOGD(TAG, "Room[%u] Current Pos: %.2f", room_id, current_pos);
            
            if (current_pos < 0 || current_pos > 1) {
                ESP_LOGI(TAG, "[%u] Outside of awake time, lights are off", current_time);
                if (room_light_status[room_id]) {
                    update_lights_power_off(room_id);
                    room_light_status[room_id] = false;
                }
            } else {
                float new_color_temp = alm_room_compute_color_temperature(current_pos);
                ESP_LOGI(TAG, "[%u] Room[%u] Color Temp: %.2f", current_time, room_id, new_color_temp);

                // Round color temp to nearest hundreth
                uint16_t rounded_color_temp = round(new_color_temp / 100) * 100;
                ESP_LOGI(TAG, "[%u] Room[%u] Rounded Color Temp: %u", current_time, room_id, rounded_color_temp);

                if (rounded_color_temp != room_last_color_temp_sent[room_id] || 
                    !room_light_status[room_id]) {
                    update_light_by_color_temperature(room_id, rounded_color_temp);
                    room_light_status[room_id] = true;
                    room_last_color_temp_sent[room_id] = rounded_color_temp;
                }
            }
        }
        vTaskDelayUntil(&x_last_wake_time, x_frequency);
    }
}

static void update_light_by_color_temperature(uint8_t room_id, uint16_t color_temperature) {
    RGBResult res = color_temp_to_rgb(color_temperature);
    if (!res.success) {
        ESP_LOGE(TAG, "Invalid Color Temperature");
        return;
    }

    QueueMessage light_msg = {
        .origin = ORIGIN_MAIN,
        .device = DEVICE_LIGHT,
        .light.action = LIGHT_SET_RGB,
        .light.payload.set_rgb = {
            .room_id = room_id,
            .r = res.value.r,
            .g = res.value.g,
            .b = res.value.b
        }
    };

    if (message_router_push_local(&light_msg) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send message to queue");
    }
}

static void update_light_by_rgb(uint8_t room_id, RGB rgb) {
    QueueMessage light_msg = {
        .origin = ORIGIN_MAIN,
        .device = DEVICE_LIGHT,
        .light.action = LIGHT_SET_RGB,
        .light.payload.set_rgb = {
            .room_id = room_id,
            .r = rgb.r,
            .g = rgb.g,
            .b = rgb.b
        }
    };

    if (message_router_push_local(&light_msg) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send message to queue");
    }
}

static void update_lights_power_off(uint8_t room_id) {
    ESP_LOGI(TAG, "Publishing Lights Off");

    QueueMessage light_msg = {
        .origin = ORIGIN_MAIN,
        .device = DEVICE_LIGHT,
        .light.action = LIGHT_SET_RGB,
        .light.payload.set_rgb = {
            .room_id = room_id,
            .r = 0,
            .g = 0,
            .b = 0
        }
    };

    if (message_router_push_local(&light_msg) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send message to queue");
    }
}

static void alm_task_create() {
    xTaskCreate(adaptive_lighting_mode, 
        "adaptive_lighting_mode", 
        4096, NULL, 4, 
        &adaptive_lighting_mode_handler);
}

static void alm_wake_and_sleep_init(const uint8_t room_id,
    const char wake_time[6], 
    const char sleep_time[6],
    const char current_time[6]) {
    uint16_t wake_time_total_minutes, 
             sleep_time_total_minutes, 
             current_time_total_minutes; 
    
    if (!hhmm_to_minutes(wake_time, &wake_time_total_minutes)) {
        ESP_LOGE(TAG, "INVALID WAKE TIME");
        return;
    }
    if (!hhmm_to_minutes(sleep_time, &sleep_time_total_minutes)) {
        ESP_LOGE(TAG, "INVALID SLEEP TIME");
        return;
    }
    if (!hhmm_to_minutes(current_time, &current_time_total_minutes)) {
        ESP_LOGE(TAG, "INVALID CURRENT TIME");
        return;
    }

    ESP_LOGD(TAG, "Wake Total: %u", wake_time_total_minutes);
    ESP_LOGD(TAG, "Sleep Total: %u", sleep_time_total_minutes);
    ESP_LOGD(TAG, "Current Time Total: %u", current_time_total_minutes);

    alm_room_wake_minutes_set(&s_state, room_id, wake_time_total_minutes);
    // For sleep times past midnight
    if (sleep_time_total_minutes < wake_time_total_minutes)
        sleep_time_total_minutes += (24 * 60); 
    alm_room_awake_period_set(&s_state, room_id, 
                    sleep_time_total_minutes - wake_time_total_minutes);
    alm_system_time_set(&s_state, current_time_total_minutes);
}

// ===== State Transisions =====
// !!!!! Mutex Unsafe !!!!!
static void alm_room_color_before_enable_set_unsafe(AlmState* state, uint8_t room_id, RGB rgb) {
    state->room_state[room_id].color_before_enable = rgb;    
}

static void alm_room_enable_unsafe(AlmState* state, uint8_t room_id) {
    state->room_state[room_id].is_enabled = true;    
}

static void alm_room_disable_unsafe(AlmState* state, uint8_t room_id) {
    state->room_state[room_id].is_enabled = false;
}

static void alm_room_wake_minutes_set_unsafe(AlmState* state, uint8_t room_id, uint16_t wake_minutes) {
    state->room_state[room_id].wake_minutes = wake_minutes;
}

static void alm_room_awake_period_set_unsafe(AlmState* state, uint8_t room_id, uint16_t awake_period) {
    state->room_state[room_id].awake_period = awake_period;
}

static void alm_system_time_set_unsafe(AlmState* state, uint16_t system_time) {
    state->system_time = system_time;
}

static void alm_system_time_increment_unsafe(AlmState* state) {
    state->system_time = (state->system_time + 1) % DAY_IN_MINUTES;
}

static void alm_task_set_running_unsafe(AlmState* state) {
    state->task_running = true;
}

static void alm_task_set_idle_unsafe(AlmState* state) {
    state->task_running = false;
}

// !!!!!!!!!!!!!!!!!!!!!!!!

// ------ Mutex Safety ------ 
static void alm_room_color_before_enable_set(AlmState* state, uint8_t room_id, RGB rgb) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_room_color_before_enable_set_unsafe(state, room_id, rgb);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_room_enable(AlmState* state, uint8_t room_id) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_room_enable_unsafe(state, room_id);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_room_disable(AlmState* state, uint8_t room_id) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_room_disable_unsafe(state, room_id);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_room_wake_minutes_set(AlmState* state, uint8_t room_id, uint16_t wake_minutes) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_room_wake_minutes_set_unsafe(state, room_id, wake_minutes);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_room_awake_period_set(AlmState* state, uint8_t room_id, uint16_t awake_period) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_room_awake_period_set_unsafe(state, room_id, awake_period);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_system_time_set(AlmState* state, uint16_t system_time) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_system_time_set_unsafe(state, system_time);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_system_time_increment(AlmState* state) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_system_time_increment_unsafe(state);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_task_set_running(AlmState* state) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_task_set_running_unsafe(state);
    xSemaphoreGive(alm_state_mutex);
}

static void alm_task_set_idle(AlmState* state) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    alm_task_set_idle_unsafe(state);
    xSemaphoreGive(alm_state_mutex);
}

// ===== State Getters =====
// !!!!! Mutex Unsafe !!!!!
static AlmState alm_get_snapshot_unsafe() {
    return s_state;
}

static AlmRoomInternal alm_room_get_unsafe(uint8_t room_id) {
    return s_state.room_state[room_id];
}

static RGB alm_room_color_before_enable_get_unsafe(uint8_t room_id) {
    return s_state.room_state[room_id].color_before_enable;
}

static bool alm_room_is_enabled_unsafe(uint8_t room_id) {
    return s_state.room_state[room_id].is_enabled;
}

static uint16_t alm_system_time_get_unsafe() {
    return s_state.system_time;
}


static bool alm_task_is_running_unsafe() {
    return s_state.task_running;
}



// !!!!!!!!!!!!!!!!!!!!!!!!

// ------ Mutex Safe ------ 
static AlmState alm_get_snapshot() {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    AlmState snapshot =  alm_get_snapshot_unsafe();
    xSemaphoreGive(alm_state_mutex);
    return snapshot;
}

static AlmRoomInternal alm_room_get(uint8_t room_id) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    AlmRoomInternal room = alm_room_get_unsafe(room_id);
    xSemaphoreGive(alm_state_mutex);
    return room;
}

static RGB alm_room_color_before_enable_get(uint8_t room_id) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    RGB color =  alm_room_color_before_enable_get_unsafe(room_id);
    xSemaphoreGive(alm_state_mutex);
    return color;
}

static bool alm_room_is_enabled(uint8_t room_id) {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    bool enabled =  alm_room_is_enabled_unsafe(room_id);
    xSemaphoreGive(alm_state_mutex);
    return enabled;
}

static uint16_t alm_system_time_get() {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    uint16_t time =  alm_system_time_get_unsafe();
    xSemaphoreGive(alm_state_mutex);
    return time;
}

static bool alm_task_is_running() {
    xSemaphoreTake(alm_state_mutex, portMAX_DELAY);
    bool enabled =  alm_task_is_running_unsafe();
    xSemaphoreGive(alm_state_mutex);
    return enabled;
}

static uint8_t alm_room_count_get() {
    uint8_t count = 0;
    AlmState snapshot = alm_get_snapshot(); 
    for (size_t i = 0; i < MAX_ROOMS; ++i) {
        if (snapshot.room_state[i].is_enabled) count++;
    } 
    return count;
}

// ===== Pure functions =====
static float alm_room_compute_position(const AlmRoomInternal* room, uint16_t time) {
    uint16_t raw_time = time % DAY_IN_MINUTES;
    uint16_t adjusted_time = raw_time;
    if (raw_time < room->wake_minutes)
        adjusted_time = raw_time + DAY_IN_MINUTES;
    float pos = ((float)(adjusted_time) - room->wake_minutes) / 
                        (float)room->awake_period; 
    return pos;
}

static float alm_room_compute_color_temperature(float pos) {
    float color_temp = MIN_COLOR_TEMP + AMPLITUDE * (1 - cosf(pos * 2 * M_PI));
    return color_temp;
}

static bool hhmm_to_minutes(const char hhmm[6], uint16_t* total_minutes) { 
    uint16_t hours, minutes;
    if (sscanf(hhmm, "%"SCNd16":%"SCNd16, &hours, &minutes) == 2) {
        if (hours >= 24 || minutes >= 60) return false;
        *total_minutes = (hours * 60) + minutes;
        return true;
    } else {
        return false;
    }
}

static bool minutes_to_hhmm(uint16_t total_minutes, char hhmm[6]) {
    uint8_t hours = total_minutes / 60;
    if (hours >= 24) return false;
    uint8_t minutes = total_minutes % 60;
    snprintf(hhmm, 6, "%02u:%02u", hours, minutes);
    return true; 
}