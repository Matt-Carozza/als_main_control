idf_component_register(SRCS "main_control.c"
                    REQUIRES protocol transport_mqtt message_router light
                    INCLUDE_DIRS "include")
