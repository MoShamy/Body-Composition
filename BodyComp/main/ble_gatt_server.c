#include "ble_service.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "ble_service";

static ble_user_profile_t s_profile = {
    .age = 0,
    .sex = BIA_SEX_FEMALE,
};

static ble_measurement_result_t s_last_result = {0};
static ble_measurement_status_t s_measurement_status = BLE_MEASUREMENT_STATUS_READY;

static ble_start_handler_t s_start_handler;
static void *s_start_context;

static const char *status_to_string(ble_measurement_status_t status)
{
    switch (status) {
    case BLE_MEASUREMENT_STATUS_IDLE:
        return "idle";
    case BLE_MEASUREMENT_STATUS_READY:
        return "ready";
    case BLE_MEASUREMENT_STATUS_MEASURING:
        return "measuring";
    case BLE_MEASUREMENT_STATUS_DONE:
        return "done";
    case BLE_MEASUREMENT_STATUS_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void ble_service_init(void)
{
    ESP_LOGI(TAG, "BLE service initialized (contract layer - NimBLE stack not yet integrated)");
    ble_service_publish_status(BLE_MEASUREMENT_STATUS_READY, 0, 0);
}

void ble_service_register_start_handler(ble_start_handler_t handler, void *context)
{
    s_start_handler = handler;
    s_start_context = context;
}

void ble_service_set_user_profile(const ble_user_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }

    s_profile = *profile;
}

bool ble_service_get_user_profile(ble_user_profile_t *profile)
{
    if (profile == NULL) {
        return false;
    }

    *profile = s_profile;
    return true;
}

bool ble_service_request_measurement(void)
{
    if (s_start_handler == NULL) {
        ESP_LOGW(TAG, "Measurement start requested but no handler is registered");
        return false;
    }

    ESP_LOGI(TAG, "Measurement start requested: age=%u sex=%u", s_profile.age, s_profile.sex);
    s_start_handler(&s_profile, s_start_context);
    return true;
}

void ble_service_publish_status(ble_measurement_status_t status, uint8_t error_code,
                                uint8_t progress_percent)
{
    s_measurement_status = status;

    ESP_LOGI(TAG, "Status=%s error=%u progress=%u%%",
             status_to_string(status), error_code, progress_percent);
}

void ble_service_publish_result(const ble_measurement_result_t *result)
{
    if (result == NULL) {
        ESP_LOGW(TAG, "Attempted to publish a NULL measurement result");
        return;
    }

    s_last_result = *result;

    ESP_LOGI(TAG,
             "Result: weight=%.2f kg height=%.2f cm Z=%.1f ohm BF=%.1f%% FFM=%.2f kg",
             result->weight_kg, result->height_cm, result->z_ohms,
             result->body_fat_pct, result->ffm_kg);
}
