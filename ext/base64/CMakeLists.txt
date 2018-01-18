add_library(BASE64 INTERFACE)

target_include_directories(BASE64 INTERFACE src)

zephyr_library()
target_include_directories(BASE64 INTERFACE
    include
    port/zephyr/include
)

zephyr_library_sources(
    src/base64.c
    src/hex.c
)

zephyr_library_link_libraries(BASE64)
target_link_libraries(BASE64 INTERFACE zephyr_interface)
