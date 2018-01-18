/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "sysinit/sysinit.h"
#include "mgmt/mgmt.h"
#include "img_mgmt/img_mgmt_impl.h"
#include "img_mgmt/img_mgmt.h"
#include "img_mgmt_priv.h"

int
img_mgmt_impl_erase_slot(void)
{
    const struct flash_area *fa;
    bool empty;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_is_empty(fa, &empty);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    if (!empty) {
        rc = flash_area_erase(fa, 0, fa->fa_size);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

int
img_mgmt_impl_write_pending(int slot, bool permanent)
{
    uint32_t image_flags;
    uint8_t state_flags;
    int split_app_active;
    int rc;

    state_flags = imgmgr_state_flags(slot);
    split_app_active = split_app_active_get();

    /* Unconfirmed slots are always runable.  A confirmed slot can only be
     * run if it is a loader in a split image setup.
     */
    if (state_flags & IMGMGR_STATE_F_CONFIRMED &&
        (slot != 0 || !split_app_active)) {

        return MGMT_ERR_EBADSTATE;
    }

    rc = imgr_read_info(slot, NULL, NULL, &image_flags);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    if (!(image_flags & IMAGE_F_NON_BOOTABLE)) {
        /* Unified image or loader. */
        if (!split_app_active) {
            /* No change in split status. */
            rc = boot_set_pending(permanent);
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }
        } else {
            /* Currently loader + app; testing loader-only. */
            if (permanent) {
                rc = split_write_split(SPLIT_MODE_LOADER);
            } else {
                rc = split_write_split(SPLIT_MODE_TEST_LOADER);
            }
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }
        }
    } else {
        /* Testing split app. */
        if (permanent) {
            rc = split_write_split(SPLIT_MODE_APP);
        } else {
            rc = split_write_split(SPLIT_MODE_TEST_APP);
        }
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

int
img_mgmt_impl_write_confirmed(void)
{
    /* Confirm the unified image or loader in slot 0. */
    rc = boot_set_confirmed();
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    /* If a split app in slot 1 is active, confirm it as well. */
    if (split_app_active_get()) {
        rc = split_write_split(SPLIT_MODE_APP);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    } else {
        rc = split_write_split(SPLIT_MODE_LOADER);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

int
img_mgmt_impl_read(int slot, unsigned int offset, void *dst,
                   unsigned int num_bytes)
{
    const struct flash_area *fa;
    int area_id;
    int rc;

    area_id = flash_area_id_from_image_slot(image_slot);
    rc = flash_area_open(area_id, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_read(fa, offset, dst, num_bytes);
    flash_area_close(fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
}

int
img_mgmt_impl_write_image_data(unsigned int offset, const void *data,
                               unsigned int num_bytes, bool last)
{
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_write(fa, offset, data, num_bytes);
    flash_area_close(fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
}

int
img_mgmt_impl_swap_type(void)
{
    switch (boot_swap_type()) {
    case BOOT_SWAP_TYPE_NONE:
        return IMG_MGMT_SWAP_TYPE_NONE;
    case BOOT_SWAP_TYPE_TEST:
        return IMG_MGMT_SWAP_TYPE_TEST;
    case BOOT_SWAP_TYPE_PERM:
        return IMG_MGMT_SWAP_TYPE_PERM;
    case BOOT_SWAP_TYPE_REVERT:
        return IMG_MGMT_SWAP_TYPE_REVERT;
    default:
        assert(0);
        return IMG_MGMT_SWAP_TYPE_NONE;
    }
}

void
img_mgmt_module_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = img_mgmt_register_group();
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(IMGMGR_CLI)
    rc = imgr_cli_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
