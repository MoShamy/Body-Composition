#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bia.h"

typedef struct {
    uint8_t age;
    bia_sex_t sex;
} ble_user_profile_t;

typedef struct {
    float weight_kg;
    float height_cm;
    float z_ohms;
    float body_fat_pct;
    float ffm_kg;
} ble_measurement_result_t;

typedef enum {
    BLE_MEASUREMENT_STATUS_IDLE = 0,
    BLE_MEASUREMENT_STATUS_READY = 1,
    BLE_MEASUREMENT_STATUS_MEASURING = 2,
    BLE_MEASUREMENT_STATUS_DONE = 3,
    BLE_MEASUREMENT_STATUS_ERROR = 4,
} ble_measurement_status_t;

typedef void (*ble_start_handler_t)(const ble_user_profile_t *profile, void *context);

void ble_service_init(void);
void ble_service_register_start_handler(ble_start_handler_t handler, void *context);
void ble_service_set_user_profile(const ble_user_profile_t *profile);
bool ble_service_get_user_profile(ble_user_profile_t *profile);
bool ble_service_request_measurement(void);
void ble_service_publish_status(ble_measurement_status_t status, uint8_t error_code,
                                uint8_t progress_percent);
void ble_service_publish_result(const ble_measurement_result_t *result);