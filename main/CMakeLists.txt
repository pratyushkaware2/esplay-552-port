idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash
                       REQUIRES esplay_input esplay_display
                       INCLUDE_DIRS "")

