#pragma once

#include <stdbool.h>

void mqtt_transport_start(void);
bool mqtt_transport_is_connected(void);
int mqtt_transport_publish(const char* topic, const char* paylaod);