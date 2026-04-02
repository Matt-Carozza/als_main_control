idf_component_register(SRCS "transport_mqtt.c"
                    PRIV_REQUIRES transport_mqtt mqtt message_router esp_wifi
                    INCLUDE_DIRS "include")
