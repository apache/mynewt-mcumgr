/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <zephyr.h>
#include <string.h>
#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include "logging/mdlog.h"
#include "logging/reboot_log.h"
#include "fcb.h"
#include "mgmt/smp_bt.h"
#include "mgmt/buf.h"

#if defined CONFIG_FCB && defined CONFIG_FILE_SYSTEM_NFFS
#error Both CONFIG_FCB and CONFIG_FILE_SYSTEM_NFFS are defined; smp_svr \
       application only supports one at a time.
#endif

#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
#include "fs_mgmt/fs_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include "os_mgmt/os_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include "img_mgmt/img_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_LOG_MGMT
#include "log_mgmt/log_mgmt.h"
#endif

#define DEVICE_NAME         CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN     (sizeof(DEVICE_NAME) - 1)

struct fcb smp_svr_fcb;
struct mdlog smp_svr_log;

/* smp_svr uses the first "peruser" log module. */
#define SMP_SVR_MDLOG_MODULE  (MDLOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the smp_svr module. */
#define SMP_SVR_MDLOG(lvl, ...) \
    MDLOG_ ## lvl(&smp_svr_log, SMP_SVR_MDLOG_MODULE, __VA_ARGS__)


static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
        0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
        0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void advertise(void)
{
    int rc;

    bt_le_adv_stop();

    rc = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                         sd, ARRAY_SIZE(sd));
    if (rc) {
        printk("Advertising failed to start (rc %d)\n", rc);
        return;
    }

    printk("Advertising successfully started\n");
}

static void connected(struct bt_conn *conn, u8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        printk("Connected\n");
    }
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);
    advertise();
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    advertise();
}

static int init_fcb(void)
{
    /* If the user has enabled FCB support, assume there is an FCB in the NFFS
     * area.
     */
#ifndef CONFIG_FCB
    return 0;
#endif

    static struct flash_sector sectors[128];

    uint32_t sector_cnt;
    int rc;

    sector_cnt = sizeof sectors / sizeof sectors[0];
    rc = flash_area_get_sectors(4, &sector_cnt, sectors);
    if (rc != 0) {
        return rc;
    }

    if (sector_cnt <= 1) {
        return -EINVAL;
    }

    /* Initialize the FCB in the NFFS flash area (area 4) with one scratch
     * sector.
     */
    smp_svr_fcb = (struct fcb) {
        .f_magic = 0x12345678,
        .f_version = MDLOG_VERSION,
        .f_sector_cnt = sector_cnt - 1,
        .f_scratch_cnt = 1,
        .f_area_id = 4,
        .f_sectors = sectors,
    };

    rc = fcb_init(&smp_svr_fcb);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

void main(void)
{
    int rc;

    rc = init_fcb();
    assert(rc == 0);
    reboot_log_configure(&smp_svr_log);

    /* Register the built-in mcumgr command handlers. */
#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
    fs_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
    os_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
    img_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_LOG_MGMT
    log_mgmt_register_group();
#endif

    /* Enable Bluetooth. */
    rc = bt_enable(bt_ready);
    if (rc != 0) {
        printk("Bluetooth init failed (err %d)\n", rc);
        return;
    }
    bt_conn_cb_register(&conn_callbacks);

    /* Initialize the Bluetooth mcumgr transport. */
    smp_bt_register();

    mdlog_register("smp_svr", &smp_svr_log, &mdlog_fcb_handler, &smp_svr_fcb,
                   MDLOG_LEVEL_DEBUG);

    /* The system work queue handles all incoming mcumgr requests.  Let the
     * main thread idle while the mcumgr server runs.
     */
    while (1) {
        k_sleep(INT32_MAX);
    }
}
