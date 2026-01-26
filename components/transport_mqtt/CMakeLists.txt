idf_component_register(SRCS "transport_mqtt.c"
                    PRIV_REQUIRES transport_mqtt mqtt message_router
                    INCLUDE_DIRS "include")
