#include "app/nvs_manager.h"

#include <inttypes.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "contador_nvs"
#define NVS_KEY_CONTADOR "contador"

static const char *TAG = "NVS";
static nvs_handle_t s_handle;
static SemaphoreHandle_t s_mutex;

esp_err_t nvs_manager_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS requiere reinicializacion");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "No se pudo borrar NVS");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "No se pudo inicializar NVS");

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle), TAG,
                        "No se pudo abrir namespace");

    int32_t value = 0;
    err = nvs_get_i32(s_handle, NVS_KEY_CONTADOR, &value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Primer arranque: creando contador=%d", NVS_CONTADOR_DEFAULT);
        ESP_RETURN_ON_ERROR(nvs_set_i32(s_handle, NVS_KEY_CONTADOR,
                                        NVS_CONTADOR_DEFAULT), TAG,
                            "No se pudo crear contador");
        ESP_RETURN_ON_ERROR(nvs_commit(s_handle), TAG, "No se pudo confirmar contador");
        value = NVS_CONTADOR_DEFAULT;
    } else {
        ESP_RETURN_ON_ERROR(err, TAG, "No se pudo leer contador");
    }

    ESP_LOGI(TAG, "Contador cargado: %" PRId32, value);
    return ESP_OK;
}

esp_err_t nvs_manager_contador_exists(bool *exists)
{
    if (exists == NULL || s_handle == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    int32_t value;
    esp_err_t err = nvs_get_i32(s_handle, NVS_KEY_CONTADOR, &value);
    *exists = err == ESP_OK;
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

esp_err_t nvs_manager_get_contador(int32_t *value)
{
    if (value == NULL || s_handle == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = nvs_get_i32(s_handle, NVS_KEY_CONTADOR, value);
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t nvs_manager_set_contador(int32_t value, bool *changed)
{
    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int32_t current = 0;
    esp_err_t err = nvs_get_i32(s_handle, NVS_KEY_CONTADOR, &current);
    if (err == ESP_OK && current == value) {
        if (changed != NULL) {
            *changed = false;
        }
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    err = nvs_set_i32(s_handle, NVS_KEY_CONTADOR, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    if (changed != NULL) {
        *changed = err == ESP_OK;
    }
    xSemaphoreGive(s_mutex);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Contador actualizado correctamente: %" PRId32, value);
    }
    return err;
}
