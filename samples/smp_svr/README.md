# smp_svr

This sample application implements a simple SMP server with the following
transports:
    * Shell
    * Bluetooth

`smp_svr` enables support for the following command groups:
    * fs_mgmt
    * img_mgmt
    * log_mgmt
    * os_mgmt

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

In addition, the MCUBoot boot loader (https://github.com/runtimeco/mcuboot) is
required for img_mgmt to function properly.

The smp_svr app logs reboots to a flash circular buffer (FCB) backed log.  The
flash map for the nRF52 only allocates flash for either the NFFS file system or
the FCB, but not both.  By default, this application uses the FCB log, not the
file system.  You can enable the NFFS file system and disable the FCB as follows-

1. In `zephyr/prj.conf`, uncomment the `FILE_SYSTEM` settings and comment out
the `FLASH` and `FCB` settings:

```
    # Enable the NFFS file system.
    CONFIG_FILE_SYSTEM=y
    CONFIG_FILE_SYSTEM_NFFS=y

     # Enable the flash circular buffer (FCB) for the reboot log.
    #CONFIG_FLASH_PAGE_LAYOUT=y
    #CONFIG_FLASH_MAP=y
    #CONFIG_FCB=y
```

2. Link in the NFFS library by uncommenting the `NFFS` line in
`zephyr/CMakeLists.txt`:

```
    zephyr_link_libraries(
        MCUMGR
        NFFS
    )
```

### Building

The Zephyr port of `smp_svr` can be built using the usual procedure:

```
$ cd samples/smp_svr/zephyr
$ mkdir build && cd build
$ cmake -DBOARD=<board> ..
$ make
```
