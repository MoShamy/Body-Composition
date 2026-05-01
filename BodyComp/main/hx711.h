#pragma once
#include "driver/gpio.h"

#define HX711_DT_PIN  GPIO_NUM_21
#define HX711_SCK_PIN GPIO_NUM_22

void hx711_init(void);
int32_t hx711_read(void);
float hx711_get_weight(int32_t offset, float scale);