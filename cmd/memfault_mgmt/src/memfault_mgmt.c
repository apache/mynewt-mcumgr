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

#include "os/mynewt.h"
#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "memfault_mgmt/memfault_mgmt.h"
#include "memfault_mgmt/memfault_mgmt_config.h"
#include "memfault/core/data_packetizer.h"
#include "memfault/core/debug_log.h"

static mgmt_handler_fn memfault_mgmt_pull;

static struct mgmt_handler memfault_mgmt_handlers[] = {
    [MEMFAULT_MGMT_ID_PULL] = { memfault_mgmt_pull, NULL },
};

#define MEMFAULT_MGMT_HANDLER_CNT \
    (sizeof(memfault_mgmt_handlers) / sizeof(memfault_mgmt_handlers[0]))

static struct mgmt_group memfault_mgmt_group = {
    .mg_handlers = memfault_mgmt_handlers,
    .mg_handlers_count = MEMFAULT_MGMT_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_MEMFAULT,
};


static const sPacketizerConfig cfg = {
    // Enable multi packet chunking. This means a chunk may span multiple calls to
    // memfault_packetizer_get_next().
    .enable_multi_packet_chunk = true,
};
static sPacketizerMetadata metadata;

/**
 * Command handler: memfault pull
 */
static int
memfault_mgmt_pull(struct mgmt_ctxt *ctxt)
{
    CborError err;
    uint8_t buf[MEMFAULT_MGMT_MAX_CHUNK_SIZE];
    size_t buf_len = sizeof(buf);

    if (!metadata.send_in_progress) {
        bool data_available = memfault_packetizer_begin(&cfg, &metadata);
        if (!data_available) {
            // there are no more chunks to send
            MEMFAULT_LOG_INFO("All data has been sent!");
            return MGMT_ERR_EOK;
        }
    }

    eMemfaultPacketizerStatus packetizer_status =
        memfault_packetizer_get_next(buf, &buf_len);
    if (packetizer_status == kMemfaultPacketizerStatus_NoMoreData) {
        MEMFAULT_LOG_ERROR("Unexpected packetizer status: %d",
                           (int)packetizer_status);
        return MGMT_ERR_EBADSTATE;
    }

    if (packetizer_status == kMemfaultPacketizerStatus_EndOfChunk) {
        metadata.send_in_progress = false;
    }

    struct cbor_attr_t attrs[] = {
        { NULL },
    };

    err = cbor_read_object(&ctxt->it, attrs);
    if (err != 0) {
        return MGMT_ERR_EINVAL;
    }

    err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
    err |= cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);
    err |= cbor_encode_text_stringz(&ctxt->encoder, "status");
    err |= cbor_encode_int(&ctxt->encoder, packetizer_status);
    err |= cbor_encode_text_stringz(&ctxt->encoder, "data");
    err |= cbor_encode_byte_string(&ctxt->encoder, buf, buf_len);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return MGMT_ERR_EOK;
}

void
memfault_mgmt_register_group(void)
{
    mgmt_register_group(&memfault_mgmt_group);
}

void
memfault_mgmt_module_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    memfault_mgmt_register_group();
}
