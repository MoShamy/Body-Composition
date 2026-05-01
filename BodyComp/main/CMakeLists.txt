idf_component_register(SRCS "hx711.c" "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES spi_flash esp_driver_gpio)

