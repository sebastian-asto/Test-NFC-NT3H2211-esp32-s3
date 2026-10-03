#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define I2C_MANAGER_SDA_GPIO GPIO_NUM_8
#define I2C_MANAGER_SCL_GPIO GPIO_NUM_9

esp_err_t i2c_manager_init(void);
esp_err_t i2c_manager_probe(uint8_t address);
size_t i2c_manager_scan(void);
esp_err_t i2c_manager_add_device(uint8_t address, uint32_t speed_hz,
                                 i2c_master_dev_handle_t *device);
esp_err_t i2c_manager_transmit(i2c_master_dev_handle_t device,
                               const uint8_t *data, size_t data_size,
                               int timeout_ms);
esp_err_t i2c_manager_write_then_read(i2c_master_dev_handle_t device,
                                      const uint8_t *write_data,
                                      size_t write_size, uint32_t delay_us,
                                      uint8_t *read_data, size_t read_size,
                                      int timeout_ms);
