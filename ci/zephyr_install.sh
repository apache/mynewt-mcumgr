#!/bin/bash -x

export ZEPHYR_BASE="${HOME}/zephyr"

ZEPHYR_SDK_RELEASE="0.11.0"
ZEPHYR_SDK="zephyr-sdk-${ZEPHYR_SDK_RELEASE}-setup.run"
ZEPHYR_SDK_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_RELEASE}/${ZEPHYR_SDK}"

CMAKE_RELEASE="3.16.3"
CMAKE="cmake-${CMAKE_RELEASE}-Linux-x86_64.sh"
CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_RELEASE}/${CMAKE}"

export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR="${HOME}/zephyr-sdk"

repos=(
    "https://github.com/zephyrproject-rtos/zephyr"
    "https://github.com/zephyrproject-rtos/hal_nordic"
    "https://github.com/zephyrproject-rtos/littlefs"
    "https://github.com/zephyrproject-rtos/tinycbor"
)

for repo in "${repos[@]}"; do
    git clone --depth=1 ${repo} "${HOME}/$(basename ${repo})"
    [[ $rc -ne 0 ]] && exit 1
done

source ${ZEPHYR_BASE}/zephyr-env.sh

download_dir=$HOME/download
mkdir -p ${download_dir}

wget -O ${download_dir}/${CMAKE} -c ${CMAKE_URL}
[[ $? -ne 0 ]] && exit 1

chmod +x ${download_dir}/${CMAKE}
${download_dir}/${CMAKE} --skip-license --prefix="${HOME}/.local" --exclude-subdir
[[ $? -ne 0 ]] && exit 1

wget -O ${download_dir}/${ZEPHYR_SDK} -c ${ZEPHYR_SDK_URL}
[[ $? -ne 0 ]] && exit 1

chmod +x ${download_dir}/${ZEPHYR_SDK}
${download_dir}/${ZEPHYR_SDK} --quiet --noprogress -- -d "${ZEPHYR_SDK_INSTALL_DIR}" -y
[[ $? -ne 0 ]] && exit 1

pip install PyYAML pyelftools pykwalify
