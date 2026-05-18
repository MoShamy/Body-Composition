#include "ble_service.h"

#include <string.h>

#include "esp_log.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_service";

/* UUIDs for BIA service and characteristics */
static const ble_uuid128_t bia_svc_uuid = BLE_UUID128_INIT(
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const ble_uuid128_t user_profile_uuid = BLE_UUID128_INIT(
    0x01, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const ble_uuid128_t measurement_start_uuid = BLE_UUID128_INIT(
    0x02, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const ble_uuid128_t status_uuid = BLE_UUID128_INIT(
    0x03, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const ble_uuid128_t result_uuid = BLE_UUID128_INIT(
    0x04, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static ble_user_profile_t s_profile = {
    .age = 0,
    .sex = BIA_SEX_FEMALE,
};

static ble_measurement_result_t s_last_result = {0};
static ble_measurement_status_t s_measurement_status = BLE_MEASUREMENT_STATUS_READY;

static ble_start_handler_t s_start_handler;
static void *s_start_context;

static uint16_t s_user_profile_handle;
static uint16_t s_measurement_start_handle;
static uint16_t s_status_handle;
static uint16_t s_result_handle;

static uint8_t own_addr_type;

static int ble_gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def bia_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &bia_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &user_profile_uuid.u,
                .access_cb = ble_gatt_svc_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &s_user_profile_handle,
            },
            {
                .uuid = &measurement_start_uuid.u,
                .access_cb = ble_gatt_svc_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &s_measurement_start_handle,
            },
            {
                .uuid = &status_uuid.u,
                .access_cb = ble_gatt_svc_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_status_handle,
            },
            {
                .uuid = &result_uuid.u,
                .access_cb = ble_gatt_svc_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_result_handle,
            },
            {0},
        },
    },
    {0},
};

static int ble_gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;

    if (attr_handle == s_user_profile_handle) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            uint8_t buf[2] = {s_profile.age, s_profile.sex};
            int rc = os_mbuf_append(ctxt->om, buf, sizeof(buf));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if (ctxt->om->om_len >= 2) {
                uint8_t *data = ctxt->om->om_data;
                s_profile.age = data[0];
                s_profile.sex = (bia_sex_t)data[1];
                int active = ble_gap_conn_active();
                ESP_LOGI(TAG, "User profile updated: age=%u sex=%u active_conns=%d",
                         s_profile.age, s_profile.sex, active);
                return 0;
            }
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
    } else if (attr_handle == s_measurement_start_handle) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if (ctxt->om->om_len >= 1) {
                uint8_t cmd = ctxt->om->om_data[0];
                if (cmd == 1) {
                    int active = ble_gap_conn_active();
                    ESP_LOGI(TAG, "Measurement start command received active_conns=%d", active);
                    if (s_start_handler != NULL) {
                        s_start_handler(&s_profile, s_start_context);
                    }
                }
                return 0;
            }
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
    } else if (attr_handle == s_status_handle) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            uint8_t buf[3] = {(uint8_t)s_measurement_status, 0, 0};
            int rc = os_mbuf_append(ctxt->om, buf, sizeof(buf));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    } else if (attr_handle == s_result_handle) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            uint8_t buf[20];
            int offset = 0;
            memcpy(&buf[offset], &s_last_result.weight_kg, 4);
            offset += 4;
            memcpy(&buf[offset], &s_last_result.height_cm, 4);
            offset += 4;
            memcpy(&buf[offset], &s_last_result.z_ohms, 4);
            offset += 4;
            memcpy(&buf[offset], &s_last_result.body_fat_pct, 4);
            offset += 4;
            memcpy(&buf[offset], &s_last_result.ffm_kg, 4);
            offset += 4;
            int rc = os_mbuf_append(ctxt->om, buf, offset);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void ble_advertise(void)
{
    struct ble_hs_adv_fields fields;
    struct ble_gap_adv_params adv_params;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen((const char *)fields.name);
    fields.name_is_complete = 1;
    fields.uuids128 = (ble_uuid128_t *)&bia_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data, rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising, rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started");
    }
}

static void ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed, rc=%d", rc);
        return;
    }
    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGI(TAG, "BLE stack reset, reason=%d", reason);
}

void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_service_init(void)
{
    int rc;
    ESP_LOGI(TAG, "Initializing NimBLE GATT server");

    /* Set callbacks before port init */
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    /* Initialize NimBLE port first */
    rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE port, rc=%d", rc);
        return;
    }

    /* Now init GAP/GATT services after port is ready */
    ble_svc_gap_init();
        ble_svc_gap_device_name_set("BodyComp");
    ble_svc_gatt_init();

    /* Count and add custom GATT services */
    rc = ble_gatts_count_cfg(bia_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services, rc=%d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(bia_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services, rc=%d", rc);
        return;
    }

    /* Start the FreeRTOS event loop */
    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "NimBLE GATT server initialized");
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
    ESP_LOGI(TAG, "Status=%u error=%u progress=%u%%",
             status, error_code, progress_percent);

    if (ble_gap_conn_active() > 0) {
        uint8_t buf[3] = {status, error_code, progress_percent};
        int active = ble_gap_conn_active();
        ESP_LOGI(TAG, "Notifying status: active_conns=%d", active);
        int rc = ble_gatts_notify_custom(0, s_status_handle, NULL);
        ESP_LOGI(TAG, "ble_gatts_notify_custom(status) rc=%d", rc);
    }
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

    if (ble_gap_conn_active() > 0) {
        int active = ble_gap_conn_active();
        ESP_LOGI(TAG, "Notifying result: active_conns=%d", active);
        int rc = ble_gatts_notify_custom(0, s_result_handle, NULL);
        ESP_LOGI(TAG, "ble_gatts_notify_custom(result) rc=%d", rc);
    }
}