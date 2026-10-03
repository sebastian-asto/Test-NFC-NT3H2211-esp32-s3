#include "app/scan_i2c.h"

#include "app/i2c_manager.h"
#include "esp_err.h"

void init_bus_i2c(void)
{
    ESP_ERROR_CHECK(i2c_manager_init());
}

void scan_i2c_device(void)
{
    (void)i2c_manager_scan();
}
