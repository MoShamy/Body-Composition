#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bia.h"
#include "hcsr04.h"
#include "hx711.h"

// Demo user profile — replace once host UART input is wired up.
#define USER_HEIGHT_CM 184.0f
#define USER_WEIGHT_KG 71.0f
#define USER_AGE       22
#define USER_SEX       BIA_SEX_MALE

#define CALIBRATION_SCALE 36000.0f
#define NUM_SAMPLES 5
#define HEIGHT_SAMPLES 5
#define SENSOR_HEIGHT_CM 200.0f

typedef struct {
    uint32_t pulse_width_us;
    float distance_cm;
} hcsr04_measurement_t;

static float get_weight_avg(void)
{
    float sum = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        int32_t raw = hx711_read();
        float weight = (float)raw / CALIBRATION_SCALE;
        sum += weight;
        printf("Weight: %.2f kg\n", weight);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return sum / NUM_SAMPLES;
}

static void run_weight_readings(void)
{
    while (1) {
        int32_t raw = hx711_read();
        float weight = (float)raw / CALIBRATION_SCALE;
        printf("Weight: %.2f kg\n", weight);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static float get_height_avg(float sensor_height_cm)
{
    float sum = 0;

    for (int i = 0; i < HEIGHT_SAMPLES; i++) {
        uint32_t pulse_width_us = 0;

        if (hcsr04_read_pulse_width(&pulse_width_us, pdMS_TO_TICKS(40))) {
            float distance_cm = (float)pulse_width_us / 58.0f;
            float height_cm = sensor_height_cm - distance_cm;

            if (height_cm < 0) {
                height_cm = 0;
            }

            sum += height_cm;
            printf("Height reading: %.2f cm\n", height_cm);
        } else {
            printf("HC-SR04 timeout: no echo received\n");
        }

        vTaskDelay(pdMS_TO_TICKS(60));
    }

    return sum / HEIGHT_SAMPLES;
}

static void weight_task(void *arg)
{
    (void)arg;
    run_weight_readings();
}

static void height_task(void *arg)
{
    (void)arg;

    while (1) {
        float average_height = get_height_avg(SENSOR_HEIGHT_CM);
        printf("Average height: %.2f cm\n", average_height);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void bia_task(void *arg)
{
    (void)arg;

    while (1) {
        float z = 0.0f;
        if (bia_measure_impedance_avg(&z)) {
            float bf = bia_estimate_body_fat_pct(z, USER_HEIGHT_CM, USER_WEIGHT_KG,
                                                 USER_AGE, USER_SEX);
            printf("BIA: |Z| = %.1f ohm | BF%% = %.1f%%\n", z, bf);
        } else {
            printf("BIA: averaged read failed\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // hx711_init();
    // hcsr04_init();
    bia_init();
    // Allow DAC/ADC hardware to stabilize before taking measurements
    vTaskDelay(pdMS_TO_TICKS(50));

    printf("HX711 ready. Taking weight readings...\n");
    printf("HC-SR04 ready. TRIG=GPIO%d, ECHO=GPIO%d\n", HCSR04_TRIG_PIN, HCSR04_ECHO_PIN);
    printf("Note: ECHO must be level-shifted to 3.3V before connecting to the ESP32.\n");
    printf("BIA ready. DAC sine on GPIO%d, ADC sense on GPIO%d.\n",
           BIA_DAC_GPIO, BIA_ADC_GPIO);
    
    // xTaskCreate(weight_task, "weight_task", 4096, NULL, 5, NULL);
    // xTaskCreate(height_task, "height_task", 4096, NULL, 5, NULL);
    xTaskCreate(bia_task,    "bia_task",    4096, NULL, 5, NULL);

    vTaskDelete(NULL);
}