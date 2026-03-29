idf_component_register(SRCS "main_control.c"
                    REQUIRES protocol transport_mqtt message_router light adaptive_lighting_mode
                    INCLUDE_DIRS "include")
