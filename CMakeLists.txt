idf_component_register(
    SRCS "src/postern.c" "src/postern_file.c"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_driver_i2s esp_driver_i2c nvs_flash)

if(CONFIG_BT_A2DP_USE_EXTERNAL_CODEC)
    message(FATAL_ERROR "Postern phase 1 feeds PCM to the built-in SBC encoder. Leave BT_A2DP_USE_EXTERNAL_CODEC unset.")
endif()
