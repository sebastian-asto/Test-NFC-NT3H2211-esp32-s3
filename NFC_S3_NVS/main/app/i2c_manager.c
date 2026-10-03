#include "app/i2c_manager.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "I2C";
static i2c_master_bus_handle_t s_bus;
static SemaphoreHandle_t s_mutex;

esp_err_t i2c_manager_init(void)
{
    if (s_bus != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MANAGER_SDA_GPIO,
        .scl_io_num = I2C_MANAGER_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&config, &s_bus);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Bus inicializado: SDA=GPIO%d, SCL=GPIO%d",
                 I2C_MANAGER_SDA_GPIO, I2C_MANAGER_SCL_GPIO);
    }
    return err;
}

esp_err_t i2c_manager_probe(uint8_t address)
{
    if (s_bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = i2c_master_probe(s_bus, address, 50);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Dispositivo detectado en 0x%02X", address);
    } else {
        ESP_LOGE(TAG, "Sin respuesta en 0x%02X: %s", address, esp_err_to_name(err));
    }
    return err;
}

size_t i2c_manager_scan(void)
{
    size_t found = 0;
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (i2c_master_probe(s_bus, address, 25) == ESP_OK) {
            ++found;
            ESP_LOGI(TAG, "Dispositivo encontrado en 0x%02X", address);
        }
    }
    ESP_LOGI(TAG, "Escaneo finalizado: %u dispositivo(s)", (unsigned)found);
    return found;
}

esp_err_t i2c_manager_add_device(uint8_t address, uint32_t speed_hz,
                                 i2c_master_dev_handle_t *device)
{
    if (s_bus == NULL || device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = speed_hz,
    };
    return i2c_master_bus_add_device(s_bus, &config, device);
}

esp_err_t i2c_manager_transmit(i2c_master_dev_handle_t device,
                               const uint8_t *data, size_t data_size,
                               int timeout_ms)
{
    if (device == NULL || data == NULL || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = i2c_master_transmit(device, data, data_size, timeout_ms);
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t i2c_manager_write_then_read(i2c_master_dev_handle_t device,
                                      const uint8_t *write_data,
                                      size_t write_size, uint32_t delay_us,
                                      uint8_t *read_data, size_t read_size,
                                      int timeout_ms)
{
    if (device == NULL || write_data == NULL || read_data == NULL || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_transmit(device, write_data, write_size, timeout_ms);
    if (err == ESP_OK) {
        esp_rom_delay_us(delay_us);
        err = i2c_master_receive(device, read_data, read_size, timeout_ms);
    }

    xSemaphoreGive(s_mutex);
    return err;
}
