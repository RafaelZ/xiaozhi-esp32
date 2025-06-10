/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef BLUFI_EXAMPLE_H_
#define BLUFI_EXAMPLE_H_

#include "esp_err.h" // For esp_err_t
#include <stdbool.h> // For bool
#include <stdint.h>  // For uint8_t, uint16_t
#include "esp_log.h" // For ESP_LOGI, ESP_LOGE

// Logging macros
#define BLUFI_EXAMPLE_TAG "BLUFI_EXAMPLE"
#define BLUFI_INFO(fmt, ...)   ESP_LOGI(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...)  ESP_LOGE(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

// Functions needed by movecall_moji_esp32s3.cc from blufi_security.c or blufi_init.c (custom handlers)
void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);
esp_err_t blufi_security_init(void); // Corrected return type
void blufi_security_deinit(void);

// Functions from blufi_init.c for custom NimBLE stack management
esp_err_t app_blufi_nimble_stack_init(void);
esp_err_t app_blufi_nimble_stack_deinit(void);

// Note: Functions like esp_blufi_host_and_cb_init, esp_blufi_adv_start, etc.,
// are part of the ESP-IDF BluFi API and should be included via esp_blufi_api.h
// in the .cc file that calls them. Only application-specific BluFi handlers/helpers
// defined in blufi_security.c or blufi_init.c should be declared here.

#ifdef __cplusplus
}
#endif

#endif /* BLUFI_EXAMPLE_H_ */
