#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#define HCSR04_TRIG_PIN GPIO_NUM_18
#define HCSR04_ECHO_PIN GPIO_NUM_19

void hcsr04_init(void);
bool hcsr04_read_pulse_width(uint32_t *pulse_width_us, TickType_t timeout_ticks);