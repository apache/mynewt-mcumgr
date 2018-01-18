add_library(TINYCBOR INTERFACE)

target_include_directories(TINYCBOR INTERFACE src)

zephyr_library()
zephyr_library_sources(
    src/cbor_buf_reader.c
    src/cbor_buf_writer.c
    src/cborencoder.c
    src/cborerrorstrings.c
    src/cborparser.c
    src/cborparser_dup_string.c
)
zephyr_library_link_libraries(TINYCBOR)
target_link_libraries(TINYCBOR INTERFACE zephyr_interface)
