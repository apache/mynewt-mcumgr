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
#include <string.h>

#include "os/mynewt.h"

#include <mgmt/mgmt.h>
#include <cborattr/cborattr.h>
#include <tinycbor/cbor.h>

#include "omp/omp_streamer.h"
/* #include <oic/oc_api.h> */

/* struct omgr_ctxt { */
/*     struct mgmt_ctxt ob_mc; */
/*     struct cbor_mbuf_reader ob_reader; */
/* }; */

/* struct omgr_state { */
/*     struct omgr_ctxt os_ctxt;		/\* CBOR buffer for MGMT task *\/ */
/* }; */

/* static struct omgr_state omgr_state; */

static int
omp_encode_mgmt_hdr(struct CborEncoder *enc, struct mgmt_hdr hdr)
{
    int rc;

    rc = cbor_encode_text_string(enc, "_h", 2);
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    hdr.nh_len = htons(hdr.nh_len);
    hdr.nh_group = htons(hdr.nh_group);

    /* Encode the MGMT header in the response. */
    rc = cbor_encode_byte_string(enc, (void *)&hdr, sizeof hdr);
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

static int
omp_send_err_rsp(struct CborEncoder *enc,
                 const struct mgmt_hdr *hdr,
                 int mgmt_status)
{
    int rc;

    rc = omp_encode_mgmt_hdr(enc, *hdr);
    if (rc != 0) {
        return rc;
    }

    rc = cbor_encode_text_stringz(enc, "rc");
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    rc = cbor_encode_int(enc, mgmt_status);
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

static int
omp_read_hdr(struct CborValue *cv, struct mgmt_hdr *out_hdr)
{
    size_t hlen;
    int rc;

    struct cbor_attr_t attrs[] = {
        [0] = {
            .attribute = "_h",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = (void *)out_hdr,
            .addr.bytestring.len = &hlen,
            .nodefault = 1,
            .len = sizeof *out_hdr,
        },
        [1] = { 0 }
    };

    rc = cbor_read_object(cv, attrs);
    if (rc != 0 || hlen != sizeof *out_hdr) {
        return MGMT_ERR_EINVAL;
    }

    out_hdr->nh_len = ntohs(out_hdr->nh_len);
    out_hdr->nh_group = ntohs(out_hdr->nh_group);

    return 0;
}

static int
omp_process_mgmt_hdr(struct mgmt_hdr *req_hdr, struct mgmt_hdr *rsp_hdr,
                     struct mgmt_ctxt *ctxt)
{
    int rc = 0;
    bool rsp_hdr_filled = false;
    const struct mgmt_handler *handler;

    handler = mgmt_find_handler(req_hdr->nh_group, req_hdr->nh_id);
    if (handler == NULL) {
        rc = MGMT_ERR_ENOENT;
        goto done;
    }

    switch (req_hdr->nh_op) {
    case MGMT_OP_READ:
        if (handler->mh_read == NULL) {
            rc = MGMT_ERR_ENOENT;
        } else {
            rsp_hdr->nh_op = MGMT_OP_READ_RSP;
            /* mgmt_evt(MGMT_EVT_OP_CMD_RECV, */
            /*          req_hdr->nh_group, */
            /*          req_hdr->nh_id, */
            /*          NULL); */
            rc = handler->mh_read(ctxt);
        }
        break;

    case MGMT_OP_WRITE:
        if (handler->mh_write == NULL) {
            rc = MGMT_ERR_ENOENT;
        } else {
            rsp_hdr->nh_op = MGMT_OP_WRITE_RSP;
            /* mgmt_evt(MGMT_EVT_OP_CMD_RECV, */
            /*          req_hdr->nh_group, */
            /*          req_hdr->nh_id, */
            /*          NULL); */
            rc = handler->mh_write(ctxt);
        }
        break;

    default:
        rc = MGMT_ERR_EINVAL;
        break;
    }
    if (rc != 0) {
        goto done;
    } else {
        rsp_hdr_filled = true;
    }

    /* Encode the MGMT header in the response. */
    rc = omp_encode_mgmt_hdr(&ctxt->encoder, *rsp_hdr);
    if (rc != 0) {
        rc = MGMT_ERR_ENOMEM;
    }

done:
    if (rc != 0) {
        if (rsp_hdr_filled) {
            rc = omp_send_err_rsp(&ctxt->encoder, rsp_hdr, rc);
        }
    }
    return rc;
}

int
omp_process_request_packet(struct omp_streamer *streamer, struct os_mbuf *m,
                           void *req)
{
    struct mgmt_ctxt ctxt;
    struct mgmt_hdr req_hdr, rsp_hdr;
    struct os_mbuf *m_rsp;
    int rc = 0;

    rc = mgmt_streamer_init_reader(&streamer->mgmt_stmr, m);
    assert(rc == 0);
    rc = cbor_parser_init(streamer->mgmt_stmr.reader, 0, &ctxt.parser, &ctxt.it);
    assert(rc == 0);

    rc = omp_read_hdr(&ctxt.it, &req_hdr);
    assert(rc == 0);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;

    }

    rc = mgmt_streamer_init_reader(&streamer->mgmt_stmr, m);
    assert(rc == 0);
    rc = cbor_parser_init(streamer->mgmt_stmr.reader, 0, &ctxt.parser, &ctxt.it);
    assert(rc == 0);


    m_rsp = mgmt_streamer_alloc_rsp(&streamer->mgmt_stmr, m);
    assert(m_rsp != NULL);

    rc = mgmt_streamer_init_writer(&streamer->mgmt_stmr, m_rsp);
    assert(rc == 0);

    //cbor_encoder_init(&streamer->rsp_encoder, streamer->mgmt_stmr.writer, 0);
    rc = cbor_encoder_create_map(streamer->rsp_encoder, //encoder
                                 &ctxt.encoder, //mapEncoder
                                 CborIndefiniteLength);
    assert(rc == 0);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;
    }

    rc = omp_process_mgmt_hdr(&req_hdr, &rsp_hdr, &ctxt);
    assert(rc == 0);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;
    }

    rc = cbor_encoder_close_container(streamer->rsp_encoder, &ctxt.encoder);
    assert(rc == 0);
    streamer->tx_rsp_cb(&ctxt, req, rc);
    return rc;
}
