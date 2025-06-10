#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"

#include "esp_blufi_api.h"
#include "esp_blufi.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"  // 相对路径引用

#include "blufi_example.h" // For BLUFI_INFO/ERROR macros and TAG

#ifndef BLUFI_DEVICE_NAME
#define BLUFI_DEVICE_NAME "MOJI_ESP32_BLUFI"
#endif

/* Library function declarations */
void ble_store_config_init(void);

// NimBLE Host task
static void bleprph_host_task(void *param) {
    ESP_LOGI(BLUFI_HELPER_TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
    ESP_LOGI(BLUFI_Helper_TAG, "BLE Host Task Ended");
    vTaskDelete(NULL);
}

static void blufi_on_reset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void blufi_on_sync(void) {
    esp_err_t ret = esp_blufi_profile_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "esp_blufi_profile_init failed: %s\n", esp_err_to_name(ret));
        return;
    }
    MODLOG_DFLT(INFO, "BluFi profile initialized");
}

// This function encapsulates NimBLE stack initialization for BluFi.
esp_err_t app_blufi_nimble_stack_init(void) {
    esp_err_t err;
    ESP_LOGI(BLUFI_Helper_TAG, "Initializing NimBLE stack for BluFi");

    err = nimble_port_init();
    if (err != ESP_OK) {
        BLUFI_ERROR("nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = blufi_on_reset;
    ble_hs_cfg.sync_cb = blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO; // Example IO capability
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;

    int rc = esp_blufi_gatt_svr_init();
    if (rc != 0) {
        BLUFI_ERROR("BluFi GATT server init failed, rc=%d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }

    rc = ble_svc_gap_device_name_set(BLUFI_DEVICE_NAME);
    if (rc != 0) {
        BLUFI_ERROR("Setting device name failed, rc=%d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }

    ble_store_config_init();

    esp_blufi_btc_init();

    nimble_port_freertos_init(bleprph_host_task);

    ESP_LOGI(BLUFI_Helper_TAG, "NimBLE stack initialization complete.");
    return ESP_OK;
}

// This function encapsulates NimBLE stack de-initialization.
esp_err_t app_blufi_nimble_stack_deinit(void) {
    ESP_LOGI(BLUFI_Helper_TAG, "De-initializing NimBLE stack for BluFi");
    esp_err_t ret = ESP_OK;

    if (nimble_port_stop() != 0) {
        BLUFI_ERROR("Failed to stop NimBLE port gracefully.");
    }

    esp_err_t profile_err = esp_blufi_profile_deinit();
    if (profile_err != ESP_OK) {
        BLUFI_ERROR("BluFi profile deinit failed: %s", esp_err_to_name(profile_err));
        ret = profile_err;
    }
    // esp_blufi_gatt_svr_deinit(); // No explicit gatt_svr_deinit in current ESP-IDF, cleanup might be internal
    esp_blufi_btc_deinit();

    nimble_port_deinit();

    ESP_LOGI(BLUFI_Helper_TAG, "NimBLE stack de-initialization attempt complete.");
    return ret;
}
