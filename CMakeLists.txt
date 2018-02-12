add_library(MCUMGR INTERFACE)

zephyr_library()
add_subdirectory(cborattr)
add_subdirectory(cmd)
add_subdirectory(ext)
add_subdirectory(mgmt)
add_subdirectory(smp)

zephyr_library_link_libraries(MCUMGR)

target_link_libraries(MCUMGR INTERFACE
  zephyr_interface
  BASE64
  TINYCBOR
  )
