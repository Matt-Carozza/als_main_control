#pragma once
#include "protocol.h"

void adaptive_lighting_mode_init();
void adaptive_lighting_mode_enable(const uint8_t room_id, 
                                const char wake_time[6], 
                                const char sleep_time[6],
                                const char current_time[6]);
void adaptive_lighting_mode_disable(const uint8_t room_id);
bool adaptive_lighting_mode_room_state_get(uint8_t room_id, AlmRoom* room);