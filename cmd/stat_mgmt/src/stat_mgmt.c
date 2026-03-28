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

#include <string.h>
#include <stdio.h>

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "stat_mgmt/stat_mgmt.h"
#include "stat_mgmt/stat_mgmt_impl.h"
#include "stat_mgmt/stat_mgmt_config.h"

static mgmt_handler_fn stat_mgmt_show;
static mgmt_handler_fn stat_mgmt_list;

static struct mgmt_handler stat_mgmt_handlers[] = {
    [STAT_MGMT_ID_SHOW] = { stat_mgmt_show, NULL },
    [STAT_MGMT_ID_LIST] = { stat_mgmt_list, NULL },
};

#define STAT_MGMT_HANDLER_CNT \
    sizeof stat_mgmt_handlers / sizeof stat_mgmt_handlers[0]

static struct mgmt_group stat_mgmt_group = {
    .mg_handlers = stat_mgmt_handlers,
    .mg_handlers_count = STAT_MGMT_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_STAT,
};

static int
stat_mgmt_cb_encode(struct stat_mgmt_entry *entry, void *arg)
{
    mgmt_cbor_encoder_t *enc;
    int err;

    enc = arg;

    err = 0;
    err |= mgmt_cbor_encode_text_z(enc, entry->name);
    err |= mgmt_cbor_encode_uint(enc, entry->value);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: stat show
 */
static int
stat_mgmt_show(struct mgmt_ctxt *ctxt)
{
    char stat_name[STAT_MGMT_MAX_NAME_LEN];
    int err;
    int rc;

    struct cbor_attr_t attrs[] = {
        {
            .attribute = "name",
            .type = CborAttrTextStringType,
            .addr.string = stat_name,
            .len = sizeof(stat_name)
        },
        { NULL },
    };

    err = cbor_read_object(&ctxt->decoder, attrs);
    if (err != 0) {
        return MGMT_ERR_EINVAL;
    }

    err  = mgmt_cbor_encode_text_z(&ctxt->encoder, "rc");
    err |= mgmt_cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);

    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "name");
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, stat_name);

    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "fields");
    err |= mgmt_cbor_map_begin(&ctxt->encoder);

    rc = stat_mgmt_impl_foreach_entry(stat_name, stat_mgmt_cb_encode,
                                      &ctxt->encoder);

    err |= mgmt_cbor_map_end(&ctxt->encoder);
    if (err != 0) {
        rc = MGMT_ERR_ENOMEM;
    }

    return rc;
}

/**
 * Command handler: stat list
 */
static int
stat_mgmt_list(struct mgmt_ctxt *ctxt)
{
    const char *group_name;
    int err;
    int rc;
    int i;

    err = 0;
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "rc");
    err |= mgmt_cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "stat_list");
    err |= mgmt_cbor_array_begin(&ctxt->encoder);

    /* Iterate the list of stat groups, encoding each group's name in the CBOR
     * array.
     */
    for (i = 0; ; i++) {
        rc = stat_mgmt_impl_get_group(i, &group_name);
        if (rc == MGMT_ERR_ENOENT) {
            /* No more stat groups. */
            break;
        } else if (rc != 0) {
            /* Error. */
            mgmt_cbor_array_end(&ctxt->encoder);
            return rc;
        }

        err |= mgmt_cbor_encode_text_z(&ctxt->encoder, group_name);
    }
    err |= mgmt_cbor_array_end(&ctxt->encoder);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;
}

void
stat_mgmt_register_group(void)
{
    mgmt_register_group(&stat_mgmt_group);
}
