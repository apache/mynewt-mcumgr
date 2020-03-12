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

#include "tinycbor/cbor.h"
#include "mgmt/endian.h"
#include "mgmt/mgmt.h"

static mgmt_on_evt_cb *evt_cb;
static struct mgmt_group *mgmt_group_list;
static struct mgmt_group *mgmt_group_list_end;

void *
mgmt_streamer_alloc_rsp(struct mgmt_streamer *streamer, const void *req)
{
    return streamer->cfg->alloc_rsp(req, streamer->cb_arg);
}

void
mgmt_streamer_trim_front(struct mgmt_streamer *streamer, void *buf, size_t len)
{
    streamer->cfg->trim_front(buf, len, streamer->cb_arg);
}

void
mgmt_streamer_reset_buf(struct mgmt_streamer *streamer, void *buf)
{
    streamer->cfg->reset_buf(buf, streamer->cb_arg);
}

int
mgmt_streamer_write_at(struct mgmt_streamer *streamer, size_t offset,
                       const void *data, int len)
{
    return streamer->cfg->write_at(streamer->writer, offset, data, len,
                                   streamer->cb_arg);
}

int
mgmt_streamer_init_reader(struct mgmt_streamer *streamer, void *buf)
{
    return streamer->cfg->init_reader(streamer->reader, buf, streamer->cb_arg);
}

int
mgmt_streamer_init_writer(struct mgmt_streamer *streamer, void *buf)
{
    return streamer->cfg->init_writer(streamer->writer, buf, streamer->cb_arg);
}

void
mgmt_streamer_free_buf(struct mgmt_streamer *streamer, void *buf)
{
    streamer->cfg->free_buf(buf, streamer->cb_arg);
}

static struct mgmt_group *
mgmt_find_group(uint16_t group_id, uint16_t command_id)
{
    struct mgmt_group *group;

    /*
     * Find the group with the specified group id, if one exists
     * check the handler for the command id and make sure
     * that is not NULL. If that is not set, look for the group
     * with a command id that is set
     */
    for (group = mgmt_group_list; group != NULL; group = group->mg_next) {
        if (group->mg_group_id == group_id) {
            if (command_id >= group->mg_handlers_count) {
                return NULL;
            }

            if (!group->mg_handlers[command_id].mh_read &&
                !group->mg_handlers[command_id].mh_write) {
                continue;
            }

            break;
        }
    }

    return group;
}

void
mgmt_register_group(struct mgmt_group *group)
{
    if (mgmt_group_list_end == NULL) {
        mgmt_group_list = group;
    } else {
        mgmt_group_list_end->mg_next = group;
    }
    mgmt_group_list_end = group;
}

const struct mgmt_handler *
mgmt_find_handler(uint16_t group_id, uint16_t command_id)
{
    const struct mgmt_group *group;

    group = mgmt_find_group(group_id, command_id);
    if (!group) {
        return NULL;
    }

    return &group->mg_handlers[command_id];
}

int
mgmt_write_rsp_status(struct mgmt_ctxt *ctxt, int errcode)
{
    int rc;

    rc = cbor_encode_text_stringz(&ctxt->encoder, "rc");
    if (rc != 0) {
        return rc;
    }

    rc = cbor_encode_int(&ctxt->encoder, errcode);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
mgmt_err_from_cbor(int cbor_status)
{
    switch (cbor_status) {
        case CborNoError:           return MGMT_ERR_EOK;
        case CborErrorOutOfMemory:  return MGMT_ERR_ENOMEM;
        default:                    return MGMT_ERR_EUNKNOWN;
    }
}

int
mgmt_ctxt_init(struct mgmt_ctxt *ctxt, struct mgmt_streamer *streamer)
{
    int rc;

    rc = cbor_parser_init(streamer->reader, 0, &ctxt->parser, &ctxt->it);
    if (rc != CborNoError) {
        return mgmt_err_from_cbor(rc);
    }

    cbor_encoder_init(&ctxt->encoder, streamer->writer, 0);

    return 0;
}

void
mgmt_ntoh_hdr(struct mgmt_hdr *hdr)
{
    hdr->nh_len = ntohs(hdr->nh_len);
    hdr->nh_group = ntohs(hdr->nh_group);
}

void
mgmt_hton_hdr(struct mgmt_hdr *hdr)
{
    hdr->nh_len = htons(hdr->nh_len);
    hdr->nh_group = htons(hdr->nh_group);
}

void
mgmt_register_evt_cb(mgmt_on_evt_cb *cb)
{
    evt_cb = cb;
}

void
mgmt_evt(uint8_t opcode, uint16_t group, uint8_t id, void *arg)
{
    if (evt_cb) {
        evt_cb(opcode, group, id, arg);
    }
}
