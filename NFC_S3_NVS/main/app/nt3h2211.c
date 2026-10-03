#include "app/nt3h2211.h"

#include <string.h>

#include "app/i2c_manager.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NT3H2211_I2C_SPEED_HZ 100000
#define NT3H2211_SESSION_BLOCK 0xFE
#define NT3H2211_SESSION_NC_REG 0x00
#define NT3H2211_SESSION_NS_REG 0x06
#define NT3H2211_USER_FIRST_BLOCK 0x01

static const char *TAG = "NT3H2211";
static i2c_master_dev_handle_t s_device;

static esp_err_t wait_for_eeprom(uint32_t timeout_ms)
{
    const TickType_t start = xTaskGetTickCount();
    do {
        uint8_t status = 0;
        esp_err_t err = nt3h2211_get_status(&status);
        if (err == ESP_OK &&
            (status & (NT3H2211_NS_RF_LOCKED | NT3H2211_NS_EEPROM_WR_BUSY)) == 0) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    } while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms));
    return ESP_ERR_TIMEOUT;
}

esp_err_t nt3h2211_init(void)
{
    if (s_device != NULL) {
        return ESP_OK;
    }
    esp_err_t err = i2c_manager_add_device(NT3H2211_I2C_ADDRESS,
                                           NT3H2211_I2C_SPEED_HZ, &s_device);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Driver inicializado en direccion 0x%02X",
                 NT3H2211_I2C_ADDRESS);
    }
    return err;
}

esp_err_t nt3h2211_read_block(uint8_t block, uint8_t data[NT3H2211_BLOCK_SIZE])
{
    if (data == NULL || s_device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_manager_write_then_read(s_device, &block, 1, 50, data,
                                       NT3H2211_BLOCK_SIZE, 100);
}

esp_err_t nt3h2211_write_block(uint8_t block,
                               const uint8_t data[NT3H2211_BLOCK_SIZE])
{
    if (data == NULL || s_device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t frame[1 + NT3H2211_BLOCK_SIZE] = {block};
    memcpy(&frame[1], data, NT3H2211_BLOCK_SIZE);
    esp_err_t err = i2c_manager_transmit(s_device, frame, sizeof(frame), 100);
    if (err == ESP_OK && block < 0xF8) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return err;
}

esp_err_t nt3h2211_read_session_register(uint8_t reg, uint8_t *value)
{
    if (value == NULL || reg > 7 || s_device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t command[] = {NT3H2211_SESSION_BLOCK, reg};
    return i2c_manager_write_then_read(s_device, command, sizeof(command), 50,
                                       value, 1, 100);
}

esp_err_t nt3h2211_write_session_register(uint8_t reg, uint8_t mask,
                                          uint8_t value)
{
    if (reg > 7 || s_device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t command[] = {NT3H2211_SESSION_BLOCK, reg, mask, value};
    return i2c_manager_transmit(s_device, command, sizeof(command), 100);
}

esp_err_t nt3h2211_get_status(uint8_t *status)
{
    return nt3h2211_read_session_register(NT3H2211_SESSION_NS_REG, status);
}

esp_err_t nt3h2211_configure_field_detect(void)
{
    /* FD_ON=00 y FD_OFF=00: nivel bajo mientras exista campo RF. */
    return nt3h2211_write_session_register(NT3H2211_SESSION_NC_REG, 0x3C, 0x00);
}

esp_err_t nt3h2211_ensure_ndef_formatted(void)
{
    ESP_RETURN_ON_ERROR(wait_for_eeprom(500), TAG, "EEPROM ocupada por RF");

    uint8_t block0[NT3H2211_BLOCK_SIZE];
    ESP_RETURN_ON_ERROR(nt3h2211_read_block(0x00, block0), TAG,
                        "No se pudo leer bloque 0");

    const uint8_t expected_cc[] = {0xE1, 0x10, 0x6D, 0x00};
    if (memcmp(&block0[12], expected_cc, sizeof(expected_cc)) == 0) {
        ESP_LOGI(TAG, "Capability Container NDEF valido");
        return ESP_OK;
    }

    /* Leer bloque 0 devuelve UID0=0x04. Al escribirlo, byte 0 cambia la
       direccion I2C; 0x55 debe codificarse como 0xAA. */
    block0[0] = (uint8_t)(NT3H2211_I2C_ADDRESS << 1);
    memcpy(&block0[12], expected_cc, sizeof(expected_cc));
    ESP_LOGW(TAG, "Inicializando Capability Container NDEF");
    ESP_RETURN_ON_ERROR(nt3h2211_write_block(0x00, block0), TAG,
                        "No se pudo inicializar CC NDEF");
    return i2c_manager_probe(NT3H2211_I2C_ADDRESS);
}

esp_err_t nt3h2211_read_ndef_area(uint8_t *data, size_t size)
{
    if (data == NULL || size == 0 || size % NT3H2211_BLOCK_SIZE != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(wait_for_eeprom(500), TAG, "EEPROM no disponible");
    for (size_t offset = 0; offset < size; offset += NT3H2211_BLOCK_SIZE) {
        ESP_RETURN_ON_ERROR(
            nt3h2211_read_block((uint8_t)(NT3H2211_USER_FIRST_BLOCK +
                                          offset / NT3H2211_BLOCK_SIZE),
                                &data[offset]),
            TAG, "Error leyendo NDEF");
    }
    return ESP_OK;
}

esp_err_t nt3h2211_write_ndef_area(const uint8_t *data, size_t size)
{
    if (data == NULL || size == 0 || size % NT3H2211_BLOCK_SIZE != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(wait_for_eeprom(500), TAG, "EEPROM no disponible");
    for (size_t offset = 0; offset < size; offset += NT3H2211_BLOCK_SIZE) {
        ESP_RETURN_ON_ERROR(
            nt3h2211_write_block((uint8_t)(NT3H2211_USER_FIRST_BLOCK +
                                           offset / NT3H2211_BLOCK_SIZE),
                                 &data[offset]),
            TAG, "Error escribiendo NDEF");
    }
    return ESP_OK;
}
