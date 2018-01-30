# Top-level CMakeLists.txt for the skeleton application.
#
# Copyright (c) 2017 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0
#
# This provides a basic application structure suitable for communication using
# mcumgr.  It can be used as a starting point for new applications.

# The default top-level application configuration is prj.conf.  You can place
# additional board-specific files in boards/${BOARD}.conf, and they will be
# merged into the configuration along with prj.conf.
macro(set_conf_file)
  if(EXISTS ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf)
    set(CONF_FILE "prj.conf ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf")
  else()
    set(CONF_FILE "prj.conf")
  endif()
endmacro()

# Zephyr uses Device Tree (DT) to describe some board hardware configuration.
#
# See the Zephyr documentation for more information on DT:
# http://docs.zephyrproject.org/devices/dts/device_tree.html
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dts.overlay")

# Standard Zephyr application boilerplate.
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

assert_exists(DTC_OVERLAY_FILE)

# This string ends up getting printed in the device console
if (NOT DEFINED FROM_WHO)
  set(FROM_WHO Zephyr)
endif()

target_sources(app PRIVATE
    src/main.c
)

zephyr_link_libraries(
    MCUMGR
    NFFS
)
