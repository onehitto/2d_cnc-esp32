cmake_minimum_required(VERSION 3.10)

set(INCLUDE_DIR inc)
set(SOURCES
    src/main.cc
)

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIR} REQUIRES freertos esp_common cnc drivers)