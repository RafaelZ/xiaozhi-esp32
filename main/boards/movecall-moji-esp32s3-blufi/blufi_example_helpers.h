#ifndef BLUFI_EXAMPLE_HELPERS_H
#define BLUFI_EXAMPLE_HELPERS_H

#include "esp_blufi_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_blufi_nimble_stack_init(void); // Or a more generic BLE stack init
void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);
void esp_blufi_adv_start(void); // Stub implementation might be needed
void esp_blufi_adv_stop(void);  // Stub implementation might be needed
// Add blufi_security_init and blufi_security_deinit as stubs for now
void blufi_security_init(void);
void blufi_security_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BLUFI_EXAMPLE_HELPERS_H */
