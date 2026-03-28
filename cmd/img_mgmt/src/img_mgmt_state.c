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

#include <assert.h>

#include "cborattr/cborattr.h"
#include "mgmt/mgmt.h"
#include "img_mgmt/img_mgmt.h"
#include "img_mgmt/image.h"
#include "img_mgmt_priv.h"
#include "img_mgmt/img_mgmt_impl.h"

/**
 * Collects information about the specified image slot.
 */
uint8_t
img_mgmt_state_flags(int query_slot)
{
    uint8_t flags;
    int swap_type;

    flags = 0;

    /* Determine if this is is pending or confirmed (only applicable for
     * unified images and loaders.
     */
    swap_type = img_mgmt_impl_swap_type(query_slot);
    switch (swap_type) {
    case IMG_MGMT_SWAP_TYPE_NONE:
        if (query_slot == IMG_MGMT_BOOT_CURR_SLOT) {
            flags |= IMG_MGMT_STATE_F_CONFIRMED;
            flags |= IMG_MGMT_STATE_F_ACTIVE;
        }
        break;

    case IMG_MGMT_SWAP_TYPE_TEST:
        if (query_slot == IMG_MGMT_BOOT_CURR_SLOT) {
            flags |= IMG_MGMT_STATE_F_CONFIRMED;
        } else {
            flags |= IMG_MGMT_STATE_F_PENDING;
        }
        break;

    case IMG_MGMT_SWAP_TYPE_PERM:
        if (query_slot == IMG_MGMT_BOOT_CURR_SLOT) {
            flags |= IMG_MGMT_STATE_F_CONFIRMED;
        } else {
            flags |= IMG_MGMT_STATE_F_PENDING | IMG_MGMT_STATE_F_PERMANENT;
        }
        break;

    case IMG_MGMT_SWAP_TYPE_REVERT:
        if (query_slot == IMG_MGMT_BOOT_CURR_SLOT) {
            flags |= IMG_MGMT_STATE_F_ACTIVE;
        } else {
            flags |= IMG_MGMT_STATE_F_CONFIRMED;
        }
        break;
    }

    /* Slot 0 is always active. */
    /* XXX: The slot 0 assumption only holds when running from flash. */
    if (query_slot == IMG_MGMT_BOOT_CURR_SLOT) {
        flags |= IMG_MGMT_STATE_F_ACTIVE;
    }

    return flags;
}

/**
 * Indicates whether any image slot is pending (i.e., whether a test swap will
 * happen on the next reboot.
 */
int
img_mgmt_state_any_pending(void)
{
    return img_mgmt_state_flags(0) & IMG_MGMT_STATE_F_PENDING ||
           img_mgmt_state_flags(1) & IMG_MGMT_STATE_F_PENDING;
}

/**
 * Indicates whether the specified slot has any flags.  If no flags are set,
 * the slot can be freely erased.
 */
int
img_mgmt_slot_in_use(int slot)
{
    uint8_t state_flags;

    state_flags = img_mgmt_state_flags(slot);
    return state_flags & IMG_MGMT_STATE_F_ACTIVE       ||
           state_flags & IMG_MGMT_STATE_F_CONFIRMED    ||
           state_flags & IMG_MGMT_STATE_F_PENDING;
}

/**
 * Sets the pending flag for the specified image slot.  That is, the system
 * will swap to the specified image on the next reboot.  If the permanent
 * argument is specified, the system doesn't require a confirm after the swap
 * occurs.
 */
int
img_mgmt_state_set_pending(int slot, int permanent)
{
    uint8_t hash[IMAGE_HASH_LEN];
    uint8_t state_flags;
    const uint8_t *hashp;
    int rc;

    state_flags = img_mgmt_state_flags(slot);

    /* Unconfirmed slots are always runable.  A confirmed slot can only be
     * run if it is a loader in a split image setup.
     */
    if (state_flags & IMG_MGMT_STATE_F_CONFIRMED && slot != 0) {
        rc = MGMT_ERR_EBADSTATE;
        goto done;
    }

    rc = img_mgmt_impl_write_pending(slot, permanent);
    if (rc != 0) {
        rc = MGMT_ERR_EUNKNOWN;
    }

done:
    /* Log the image hash if we know it. */
    if (img_mgmt_read_info(slot, NULL, hash, NULL)) {
        hashp = NULL;
    } else {
        hashp = hash;
    }

    if (permanent) {
        (void) img_mgmt_impl_log_confirm(rc, hashp);
    } else {
        (void) img_mgmt_impl_log_pending(rc, hashp);
    }

    return rc;
}

/**
 * Confirms the current image state.  Prevents a fallback from occurring on the
 * next reboot if the active image is currently being tested.
 */
int
img_mgmt_state_confirm(void)
{
    int rc;

    /* Confirm disallowed if a test is pending. */
    if (img_mgmt_state_any_pending()) {
        rc = MGMT_ERR_EBADSTATE;
        goto err;
    }

    rc = img_mgmt_impl_write_confirmed();
    if (rc != 0) {
        rc = MGMT_ERR_EUNKNOWN;
    }

     img_mgmt_dfu_confirmed();
err:
    return img_mgmt_impl_log_confirm(rc, NULL);
}

/**
 * Command handler: image state read
 */
int
img_mgmt_state_read(struct mgmt_ctxt *ctxt)
{
    char vers_str[IMG_MGMT_VER_MAX_STR_LEN];
    uint8_t hash[IMAGE_HASH_LEN]; /* SHA256 hash */
    struct image_version ver;
    int err;
    uint32_t flags;
    uint8_t state_flags;
    int rc;
    int i;

    err = 0;
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "images");
    err |= mgmt_cbor_array_begin(&ctxt->encoder);

    for (i = 0; i < 2 * IMG_MGMT_UPDATABLE_IMAGE_NUMBER; i++) {
        rc = img_mgmt_read_info(i, &ver, hash, &flags);
        if (rc != 0) {
            continue;
        }

        state_flags = img_mgmt_state_flags(i);

        err |= mgmt_cbor_map_begin(&ctxt->encoder);

#if IMG_MGMT_UPDATABLE_IMAGE_NUMBER > 1
        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "image");
        err |= mgmt_cbor_encode_int(&ctxt->encoder, i >> 1);
#endif
        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "slot");
        err |= mgmt_cbor_encode_int(&ctxt->encoder, i % 2);

        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "version");
        img_mgmt_ver_str(&ver, vers_str);
        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, vers_str);

        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "hash");
        err |= mgmt_cbor_encode_bytes(&ctxt->encoder, hash, IMAGE_HASH_LEN);

        if (!IMG_MGMT_FRUGAL_LIST || !(flags & IMAGE_F_NON_BOOTABLE)) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "bootable");
            err |= mgmt_cbor_encode_bool(&ctxt->encoder,
                                         !(flags & IMAGE_F_NON_BOOTABLE));
        }

        if (!IMG_MGMT_FRUGAL_LIST || (state_flags & IMG_MGMT_STATE_F_PENDING)) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "pending");
            err |= mgmt_cbor_encode_bool(&ctxt->encoder,
                                         state_flags & IMG_MGMT_STATE_F_PENDING);
        }

        if (!IMG_MGMT_FRUGAL_LIST ||
            (state_flags & IMG_MGMT_STATE_F_CONFIRMED)) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "confirmed");
            err |= mgmt_cbor_encode_bool(&ctxt->encoder,
                                         state_flags & IMG_MGMT_STATE_F_CONFIRMED);
        }

        if (!IMG_MGMT_FRUGAL_LIST || (state_flags & IMG_MGMT_STATE_F_ACTIVE)) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "active");
            err |= mgmt_cbor_encode_bool(&ctxt->encoder,
                                         state_flags & IMG_MGMT_STATE_F_ACTIVE);
        }

        if (!IMG_MGMT_FRUGAL_LIST ||
            (state_flags & IMG_MGMT_STATE_F_PERMANENT)) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "permanent");
            err |= mgmt_cbor_encode_bool(&ctxt->encoder,
                                         state_flags & IMG_MGMT_STATE_F_PERMANENT);
        }

        err |= mgmt_cbor_map_end(&ctxt->encoder);
    }

    err |= mgmt_cbor_array_end(&ctxt->encoder);

    /* splitStatus is always 0 so in frugal list it is not present at all */
    if (!IMG_MGMT_FRUGAL_LIST) {
        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "splitStatus");
        err |= mgmt_cbor_encode_int(&ctxt->encoder, 0);
    }

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: image state write
 */
int
img_mgmt_state_write(struct mgmt_ctxt *ctxt)
{
    /*
     * We add 1 to the 32-byte hash buffer as _cbor_value_copy_string() adds
     * a null character at the end of the buffer.
     */
    uint8_t hash[IMAGE_HASH_LEN + 1];
    size_t hash_len;
    bool confirm;
    int slot;
    int rc;

    const struct cbor_attr_t write_attr[] = {
        [0] = {
            .attribute = "hash",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = hash,
            .addr.bytestring.len = &hash_len,
            .len = sizeof(hash),
        },
        [1] = {
            .attribute = "confirm",
            .type = CborAttrBooleanType,
            .addr.boolean = &confirm,
            .dflt.boolean = false,
        },
        [2] = { 0 },
    };

    hash_len = 0;
    rc = cbor_read_object(&ctxt->decoder, write_attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    /* Determine which slot is being operated on. */
    if (hash_len == 0) {
        if (confirm) {
            slot = IMG_MGMT_BOOT_CURR_SLOT;
        } else {
            /* A 'test' without a hash is invalid. */
            return MGMT_ERR_EINVAL;
        }
    } else {
        slot = img_mgmt_find_by_hash(hash, NULL);
        if (slot < 0) {
            return MGMT_ERR_EINVAL;
        }
    }

    if (slot == IMG_MGMT_BOOT_CURR_SLOT && confirm) {
        /* Confirm current setup. */
        rc = img_mgmt_state_confirm();
    } else {
        rc = img_mgmt_state_set_pending(slot, confirm);
    }
    if (rc != 0) {
        return rc;
    }

    /* Send the current image state in the response. */
    rc = img_mgmt_state_read(ctxt);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
