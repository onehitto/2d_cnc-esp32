set(INCLUDE_DIR inc)
set(SOURCES
    src/webserver.cc
    src/storage.cc
    src/cnc_timer.cc
)

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIR} REQUIRES esp_http_server esp_event esp_wifi esp_common nvs_flash esp-tls esp_hw_support components spiffs driver)