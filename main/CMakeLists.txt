idf_component_register(SRCS "app_main.c"
                    PRIV_REQUIRES mqtt nvs_flash esp_netif message_router transport_mqtt protocol mobile_app light main_control occ_sensor adaptive_lighting_mode
                    INCLUDE_DIRS "include")
