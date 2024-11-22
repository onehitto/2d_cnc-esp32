set(INCLUDE_DIR inc)
set(SOURCES
    src/webserver.cc
)

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIR} REQUIRES REQUIRES esp_netif esp_event esp_wifi esp_common nvs_flash)