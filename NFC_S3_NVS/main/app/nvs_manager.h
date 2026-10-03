#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define NVS_CONTADOR_DEFAULT 20

esp_err_t nvs_manager_init(void);
esp_err_t nvs_manager_contador_exists(bool *exists);
esp_err_t nvs_manager_get_contador(int32_t *value);
esp_err_t nvs_manager_set_contador(int32_t value, bool *changed);
