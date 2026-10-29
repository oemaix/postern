idf_component_register(
    SRCS "src/postern.c" "src/postern_file.c" "src/postern_encode.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES bt esp_driver_i2s esp_driver_i2c nvs_flash esp_audio_codec)

if(NOT CONFIG_BT_A2DP_USE_EXTERNAL_CODEC OR NOT CONFIG_BT_A2DP_CODEC_AAC_ENABLED)
    message(FATAL_ERROR "Postern encodes AAC and SBC outside Bluedroid. Set BT_A2DP_USE_EXTERNAL_CODEC and BT_A2DP_CODEC_AAC_ENABLED.")
endif()
if(CONFIG_BT_A2DP_SEP_NUM_MAX LESS 2)
    message(FATAL_ERROR "Postern registers an AAC endpoint and an SBC endpoint. Set BT_A2DP_SEP_NUM_MAX to at least 2.")
endif()
