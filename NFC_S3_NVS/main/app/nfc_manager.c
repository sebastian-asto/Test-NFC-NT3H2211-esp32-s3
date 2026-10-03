#include "app/nfc_manager.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/nt3h2211.h"
#include "app/nvs_manager.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NFC_FD_GPIO GPIO_NUM_18
#define NDEF_AREA_SIZE 32

static const char *TAG = "NFC";
static TaskHandle_t s_event_task;

static void IRAM_ATTR field_detect_isr(void *arg)
{
    (void)arg;
    BaseType_t high_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_event_task, &high_priority_task_woken);
    portYIELD_FROM_ISR(high_priority_task_woken);
}

static esp_err_t parse_contador_ndef(const uint8_t *area, size_t size,
                                    int32_t *contador)
{
    if (area == NULL || contador == NULL || size < 10 || area[0] != 0x03) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const size_t ndef_length = area[1];
    if (ndef_length < 7 || ndef_length + 2 > size) {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *record = &area[2];
    if ((record[0] & 0xD7) != 0xD1 || record[1] != 1 || record[3] != 'T') {
        return ESP_ERR_NOT_SUPPORTED;
    }

    const size_t payload_length = record[2];
    if (payload_length < 2 || 4 + payload_length > ndef_length) {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *payload = &record[4];
    const size_t language_length = payload[0] & 0x3F;
    if ((payload[0] & 0x80) != 0 || language_length + 1 >= payload_length) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    const uint8_t *text = &payload[1 + language_length];
    const size_t text_length = payload_length - 1 - language_length;
    const char prefix[] = "contador=";
    if (text_length <= sizeof(prefix) - 1 ||
        memcmp(text, prefix, sizeof(prefix) - 1) != 0 || text_length >= 31) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    char number[16] = {0};
    const size_t number_length = text_length - (sizeof(prefix) - 1);
    if (number_length == 0 || number_length >= sizeof(number)) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(number, text + sizeof(prefix) - 1, number_length);

    char *end = NULL;
    long parsed = strtol(number, &end, 10);
    if (end == number || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *contador = (int32_t)parsed;
    return ESP_OK;
}

esp_err_t nfc_manager_publish_contador(int32_t contador)
{
    char text[32];
    int text_length = snprintf(text, sizeof(text), "contador=%" PRId32, contador);
    if (text_length <= 0 || text_length >= (int)sizeof(text)) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t area[NDEF_AREA_SIZE] = {0};
    const uint8_t payload_length = (uint8_t)(3 + text_length);
    const uint8_t record_length = (uint8_t)(4 + payload_length);
    size_t pos = 0;
    area[pos++] = 0x03;
    area[pos++] = record_length;
    area[pos++] = 0xD1;
    area[pos++] = 0x01;
    area[pos++] = payload_length;
    area[pos++] = 'T';
    area[pos++] = 0x02;
    area[pos++] = 'e';
    area[pos++] = 's';
    memcpy(&area[pos], text, (size_t)text_length);
    pos += (size_t)text_length;
    area[pos] = 0xFE;

    esp_err_t err = nt3h2211_write_ndef_area(area, sizeof(area));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NDEF publicado: %s", text);
    }
    return err;
}

static void process_completed_rf_session(void)
{
    uint8_t area[NDEF_AREA_SIZE];
    esp_err_t err = nt3h2211_read_ndef_area(area, sizeof(area));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo leer NDEF: %s", esp_err_to_name(err));
        return;
    }

    int32_t received = 0;
    err = parse_contador_ndef(area, sizeof(area), &received);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NDEF ignorado: formato contador invalido (%s)",
                 esp_err_to_name(err));
        return;
    }

    int32_t current = 0;
    if (nvs_manager_get_contador(&current) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo consultar contador NVS");
        return;
    }

    if (received == current) {
        ESP_LOGI(TAG, "Lectura NFC completada: contador=%" PRId32, current);
        return;
    }

    ESP_LOGI(TAG, "Nuevo contador recibido: %" PRId32, received);
    bool changed = false;
    err = nvs_manager_set_contador(received, &changed);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error actualizando NVS: %s", esp_err_to_name(err));
        return;
    }
    if (changed) {
        err = nfc_manager_publish_contador(received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS actualizado, pero fallo republicar NDEF: %s",
                     esp_err_to_name(err));
        }
    }
}

static void nfc_event_task(void *arg)
{
    (void)arg;
    int previous_level = gpio_get_level(NFC_FD_GPIO);
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(15));
        const int level = gpio_get_level(NFC_FD_GPIO);
        if (level == previous_level) {
            continue;
        }
        previous_level = level;

        if (level == 0) {
            ESP_LOGI(TAG, "Campo RF detectado; sesion NFC iniciada");
        } else {
            ESP_LOGI(TAG, "Sesion NFC finalizada");
            vTaskDelay(pdMS_TO_TICKS(10));
            process_completed_rf_session();
        }
    }
}

esp_err_t nfc_manager_init(void)
{
    ESP_RETURN_ON_ERROR(nt3h2211_ensure_ndef_formatted(), TAG,
                        "No se pudo preparar NDEF");
    ESP_RETURN_ON_ERROR(nt3h2211_configure_field_detect(), TAG,
                        "No se pudo configurar Field Detect");

    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << NFC_FD_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "Error configurando GPIO18");

    if (xTaskCreate(nfc_event_task, "nfc_event", 4096, NULL, 8,
                    &s_event_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(NFC_FD_GPIO, field_detect_isr, NULL),
                        TAG, "No se pudo registrar ISR");
    ESP_LOGI(TAG, "Field Detect configurado en GPIO18 (activo en bajo)");
    return ESP_OK;
}
