#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define NT3H2211_I2C_ADDRESS 0x55
#define NT3H2211_BLOCK_SIZE 16

#define NT3H2211_NS_NDEF_DATA_READ   (1U << 7)
#define NT3H2211_NS_I2C_LOCKED       (1U << 6)
#define NT3H2211_NS_RF_LOCKED        (1U << 5)
#define NT3H2211_NS_SRAM_I2C_READY   (1U << 4)
#define NT3H2211_NS_SRAM_RF_READY    (1U << 3)
#define NT3H2211_NS_EEPROM_WR_ERR    (1U << 2)
#define NT3H2211_NS_EEPROM_WR_BUSY   (1U << 1)
#define NT3H2211_NS_RF_FIELD_PRESENT (1U << 0)

esp_err_t nt3h2211_init(void);
esp_err_t nt3h2211_read_block(uint8_t block, uint8_t data[NT3H2211_BLOCK_SIZE]);
esp_err_t nt3h2211_write_block(uint8_t block,
                               const uint8_t data[NT3H2211_BLOCK_SIZE]);
esp_err_t nt3h2211_read_session_register(uint8_t reg, uint8_t *value);
esp_err_t nt3h2211_write_session_register(uint8_t reg, uint8_t mask,
                                          uint8_t value);
esp_err_t nt3h2211_get_status(uint8_t *status);
esp_err_t nt3h2211_configure_field_detect(void);
esp_err_t nt3h2211_ensure_ndef_formatted(void);
esp_err_t nt3h2211_read_ndef_area(uint8_t *data, size_t size);
esp_err_t nt3h2211_write_ndef_area(const uint8_t *data, size_t size);
