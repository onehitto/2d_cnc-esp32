
set(SOURCES
    cnc.cc
)

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS . REQUIRES drivers)