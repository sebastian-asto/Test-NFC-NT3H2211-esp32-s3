#include <inttypes.h>

#include "app/i2c_manager.h"
#include "app/nfc_manager.h"
#include "app/nt3h2211.h"
#include "app/nvs_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

static void contador_monitor_task(void *arg)
{
    (void)arg;
    while (true) {
        int32_t contador = 0;
        esp_err_t err = nvs_manager_get_contador(&contador);
        if (err == ESP_OK) {
            ESP_LOGI("NVS", "Contador actual: %" PRId32, contador);
        } else {
            ESP_LOGE("NVS", "No se pudo leer contador: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando PoC NFC <-> I2C <-> NVS");

    ESP_ERROR_CHECK(nvs_manager_init());
    ESP_ERROR_CHECK(i2c_manager_init());
    ESP_ERROR_CHECK(i2c_manager_probe(NT3H2211_I2C_ADDRESS));
    ESP_ERROR_CHECK(nt3h2211_init());

    int32_t contador = 0;
    ESP_ERROR_CHECK(nvs_manager_get_contador(&contador));
    ESP_ERROR_CHECK(nfc_manager_init());
    ESP_ERROR_CHECK(nfc_manager_publish_contador(contador));

    if (xTaskCreate(contador_monitor_task, "contador_monitor", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear contador_monitor_task");
    }
}
