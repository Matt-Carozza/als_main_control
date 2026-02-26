idf_component_register(SRCS "app_main.c"
                    PRIV_REQUIRES mqtt nvs_flash esp_netif message_router transport_mqtt protocol mobile_app light main_control
                    INCLUDE_DIRS "include")
