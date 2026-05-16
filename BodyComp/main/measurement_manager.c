#include "measurement_manager.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hcsr04.h"
#include "hx711.h"

static const char *TAG = "measurement_mgr";

#define CALIBRATION_SCALE 36000.0f
#define SENSOR_HEIGHT_CM  200.0f
#define WEIGHT_SAMPLES    5
#define HEIGHT_SAMPLES    5

typedef struct {
    ble_user_profile_t profile;
} measurement_job_t;

static QueueHandle_t s_job_queue;

static bool measure_weight(float *weight_kg)
{
    float sum = 0.0f;

    for (int i = 0; i < WEIGHT_SAMPLES; i++) {
        int32_t raw = hx711_read();
        float reading = (float)raw / CALIBRATION_SCALE;
        sum += reading;
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    *weight_kg = sum / (float)WEIGHT_SAMPLES;
    return true;
}

static bool measure_height(float *height_cm)
{
    float sum = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < HEIGHT_SAMPLES; i++) {
        uint32_t pulse_width_us = 0;

        if (hcsr04_read_pulse_width(&pulse_width_us, pdMS_TO_TICKS(40))) {
            float distance_cm = (float)pulse_width_us / 58.0f;
            float sample_height_cm = SENSOR_HEIGHT_CM - distance_cm;

            if (sample_height_cm < 0.0f) {
                sample_height_cm = 0.0f;
            }

            sum += sample_height_cm;
            valid_samples++;
        }

        vTaskDelay(pdMS_TO_TICKS(60));
    }

    if (valid_samples == 0) {
        return false;
    }

    *height_cm = sum / (float)valid_samples;
    return true;
}

static void measurement_start_handler(const ble_user_profile_t *profile, void *context)
{
    (void)context;

    if (profile == NULL || s_job_queue == NULL) {
        return;
    }

    measurement_job_t job = { /* create a new measurement job, containign user profile aka age,sex*/
        .profile = *profile,
    };

    xQueueOverwrite(s_job_queue, &job); /* overwrite the existing job with the new one */
}

static void measurement_task(void *arg)
{
    (void)arg;

    for (;;) {
        measurement_job_t job;

        if (xQueueReceive(s_job_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ble_service_publish_status(BLE_MEASUREMENT_STATUS_MEASURING, 0, 0);

        ble_measurement_result_t result = {
            .weight_kg = 0.0f,
            .height_cm = 0.0f,
            .z_ohms = 0.0f,
            .body_fat_pct = 0.0f,
            .ffm_kg = 0.0f,
        };

        bool weight_ok = measure_weight(&result.weight_kg);
        ble_service_publish_status(BLE_MEASUREMENT_STATUS_MEASURING, 0, 33);

        bool height_ok = measure_height(&result.height_cm);
        ble_service_publish_status(BLE_MEASUREMENT_STATUS_MEASURING, 0, 66);

        bool bia_ok = bia_measure_impedance_avg(&result.z_ohms);
        ble_service_publish_status(BLE_MEASUREMENT_STATUS_MEASURING, 0, 100);

        if (!weight_ok || !height_ok || !bia_ok) {
            ESP_LOGW(TAG, "Measurement cycle failed: weight=%d height=%d bia=%d",
                     weight_ok, height_ok, bia_ok);
            ble_service_publish_status(BLE_MEASUREMENT_STATUS_ERROR, 1, 100);
            continue;
        }

        result.body_fat_pct = bia_estimate_body_fat_pct(result.z_ohms,
                                                        result.height_cm,
                                                        result.weight_kg,
                                                        job.profile.age,
                                                        job.profile.sex);
        result.ffm_kg = result.weight_kg * (1.0f - result.body_fat_pct / 100.0f);

        ble_service_publish_result(&result);
        ble_service_publish_status(BLE_MEASUREMENT_STATUS_DONE, 0, 100);
    }
}

void measurement_manager_init(void)
{
    if (s_job_queue != NULL) {
        return;
    }

    s_job_queue = xQueueCreate(1, sizeof(measurement_job_t)); /* making a queue that should only have size 1. contains one measurement job at a time */
    if (s_job_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create measurement queue");
        return;
    }

    ble_service_register_start_handler(measurement_start_handler, NULL);
    xTaskCreate(measurement_task, "measurement_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Measurement manager ready");
}