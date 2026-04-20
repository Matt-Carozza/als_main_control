idf_component_register(SRCS "mobile_app.c"
                    REQUIRES protocol transport_mqtt light adaptive_lighting_mode message_router
                    INCLUDE_DIRS "include")
