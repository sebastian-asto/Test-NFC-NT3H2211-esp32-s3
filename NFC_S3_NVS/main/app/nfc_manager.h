#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t nfc_manager_init(void);
esp_err_t nfc_manager_publish_contador(int32_t contador);
