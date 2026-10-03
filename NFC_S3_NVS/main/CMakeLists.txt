idf_component_register(
    SRCS
        "main.c"
        "app/i2c_manager.c"
        "app/nfc_manager.c"
        "app/nt3h2211.c"
        "app/nvs_manager.c"
        "app/scan_i2c.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c nvs_flash
)
