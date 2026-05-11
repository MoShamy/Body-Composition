idf_component_register(SRCS "bia.c" "hx711.c" "hcsr04.c" "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES spi_flash esp_driver_gpio esp_driver_gptimer
                                  esp_driver_dac esp_adc)

