idf_component_register(SRCS "adaptive_lighting_mode.c"
                    PRIV_REQUIRES protocol color_temp_to_rgb_lut message_router
                    INCLUDE_DIRS "include")
