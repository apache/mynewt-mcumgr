#!/bin/bash -x

export ZEPHYR_BASE=$HOME/zephyr

export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=$HOME/zephyr-sdk

# This repo - cloned by travis
MCUMGR="${HOME}/build/apache/mynewt-mcumgr"

# Cloned by zephyr_install.sh
NRFX="${HOME}/hal_nordic"
LITTLEFS="${HOME}/littlefs"
TINYCBOR="${HOME}/tinycbor"

ZEPHYR_MODULES="${NRFX};${MCUMGR};${LITTLEFS};${TINYCBOR}"
BUILD_DIR="${HOME}/build"

apps=(
    "${ZEPHYR_BASE}/samples/subsys/mgmt/mcumgr/smp_svr"
    "${MCUMGR}/samples/smp_svr/zephyr"
)

for app in "${apps[@]}"; do
    mkdir -p "${BUILD_DIR}" && pushd "${BUILD_DIR}"

    ${HOME}/.local/bin/cmake -DBOARD=nrf52840_pca10056 -DZEPHYR_MODULES="${ZEPHYR_MODULES}" \
        "${app}"
    [[ $? -ne 0 ]] && exit 1

    make -j4
    [[ $? -ne 0 ]] && exit 1

    popd && rm -rf "${BUILD_DIR}"
done

exit 0
