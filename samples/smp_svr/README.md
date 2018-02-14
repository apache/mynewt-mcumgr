# smp_svr

This sample application implements a simple SMP server with the following
transports:
    * Shell
    * Bluetooth

`smp_svr` enables support for the following command groups:
    * fs_mgmt
    * img_mgmt
    * log_mgmt (Mynewt only)
    * os_mgmt
    * stat_mgmt

## Mynewt

The Mynewt port of `smp_svr` is a regular Mynewt app.  The below example uses the nRF52dk as the BSP, so you may need to adjust accordingly if you are using different hardware.

0. Create a Mynewt project and upload the Apache mynewt boot loader to your
board as described here: http://mynewt.apache.org/latest/os/tutorials/nRF52/

1. Add the mcumgr repo to your `project.yml` file:

```
repository.mynewt-mcumgr:
    type: git
    vers: 0-latest
    url: 'git@github.com:apache/mynewt-mcumgr.git'
```

2. Create a target that ties the `smp_svr` app to your BSP of choice (nRF52dk in this example):

```
$ newt target create smp_svr-nrf52dk
$ newt target set smp_svr-nrf52dk app=@mynewt-mcumgr/samples/smp_svr/apache \
                                  bsp=@apache-mynewt-core/hw/bsp/nrf52dk    \
                                  build_profile=debug
```

3. Build, upload, and run the application on your board:

```
$ newt run smp_svr-nrf52dk
```

## Zephyr

The Zephyr port of `smp_svr` is configured to run on an nRF52 MCU.  The
application should build and run for other platforms without modification, but
the file system management commands will not work.  To enable file system
management for a different platform, adjust the `CONFIG_FS_NFFS_FLASH_DEV_NAME`
setting in `prj.conf` accordingly.

In addition, the MCUboot boot loader (https://github.com/runtimeco/mcuboot) is
required for img_mgmt to function properly.

### Building

The below steps describe how to build and run the `smp_svr` sample app in
Zephyr.  Where examples are given, they assume the following setup:

* BSP: Nordic nRF52dk
* MCU: PCA10040

If you would like to use a more constrained platform, such as the nRF51, you
should use the `prj.conf.tiny` configuration file rather than the default
`prj.conf`.

#### Step 1: Configure environment

Define the `BOARD`, `ZEPHYR_TOOLCHAIN_VARIANT`, and any other environment
variables required by your setup.  For our example, we use the following:

```
export ZEPHYR_TOOLCHAIN_VARIANT=gccarmemb
export BOARD=nrf52_pca10040
export GCCARMEMB_TOOLCHAIN_PATH=/usr/local/Caskroom/gcc-arm-embedded/6-2017-q2-update/gcc-arm-none-eabi-6-2017-q2-update
```

#### Step 2: Build MCUboot

Build MCUboot by following the instructions in its `docs/readme-zephyr.md`
file.

#### Step 3: Upload MCUboot

Upload the resulting `zephyr.bin` file to address 0 of your board.  This can be
done in gdb as follows:

```
restore <path-to-mcuboot-zephyr.bin> binary 0
```

#### Step 4: Build smp_svr

The Zephyr port of `smp_svr` can be built using the usual procedure:

```
$ cd samples/smp_svr/zephyr
$ mkdir build && cd build
$ cmake ..
$ make
```

#### Step 5: Create an MCUboot-compatible image

Using MCUboot's `imgtool.py` script, convert the `zephyr.bin` file from step 4
into an image file.  In the below example, the MCUboot repo is located at `~/repos/mcuboot`.

```
~/repos/mcuboot/scripts/imgtool.py sign \
    --header-size 0x200 \
    --align 8 \
    --version 1.0 \
    --included-header \
    --key ~/repos/mcuboot/root-rsa-2048.pem \
    <path-to-smp_svr-zephyr.bin> signed.img
```

The above command creates an image file called `signed.img` in the current
directory.

#### Step 6: Upload the smp_svr image

Upload the `signed.img` file from step 5 to image slot 0 of your board.  The location of image slot 0 varies by BSP.  For the nRF52dk, slot 0 is located at address 0xc000.

The following gdb command uploads the image to 0xc000:
```
restore <path-to-signed.img> binary 0xc000
```

#### Step 7: Run it!

The `smp_svr` app is ready to run.  Just reset your board and test the app with the mcumgr CLI tool:

```
$ mcumgr --conntype ble --connstring peer_name=Zephyr echo hello
hello
```
