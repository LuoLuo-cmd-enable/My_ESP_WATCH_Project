idf_component_register(
    SRCS "deepseek_ai.c"
    INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/main"
    REQUIRES esp_http_client
             json
             esp_wifi
             mbedtls
             lvgl
             bsp
)
