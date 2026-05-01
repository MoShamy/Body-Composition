#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hx711.h"

#define CALIBRATION_SCALE 36000.0f // <-- replace this after calibration

#define NUM_SAMPLES 5

float get_weight_avg(void) {
    float sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int32_t raw = hx711_read();
        sum += (float)raw / CALIBRATION_SCALE;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return sum / NUM_SAMPLES;
}

void app_main(void) {
    hx711_init();

    printf("HX711 ready. Taking weight readings...\n");

    // Take offset reading with no weight (tare)
    vTaskDelay(pdMS_TO_TICKS(500));

    float weight = get_weight_avg();
    printf("Weight: %.2f kg\n", weight);
}