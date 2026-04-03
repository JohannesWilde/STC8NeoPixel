# Specifics

add_library(Specifics_millis
    specifics_millis.c
)

add_library(Specifics INTERFACE)

target_link_libraries(Specifics
    INTERFACE Specifics_millis
)
