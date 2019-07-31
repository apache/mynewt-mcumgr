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

/**
 * @file
 * @brief OMP - OIC Management Protocol.
 * XXX: REVIEW THIS
 *
 * OMP is an OIC implementation of SMP, a basic protocol that sits on top of
 * the mgmt layer. SMP requests and responses have the following format:
 *
 *     [Offset 0]: Mgmt header
 *     [Offset 8]: CBOR map of command-specific key-value pairs.
 *
 * SMP request packets may contain multiple concatenated requests.  Each
 * request must start at an offset that is a multiple of 4, so padding should
 * be inserted between requests as necessary.  Requests are processed
 * sequentially from the start of the packet to the end.  Each response is sent
 * individually in its own packet.  If a request elicits an error response,
 * processing of the packet is aborted.
 */

#ifndef H_OMP_
#define H_OMP_

#include "mgmt/mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct omp_streamer;
struct mgmt_hdr;

/** @typedef smp_tx_rsp_fn
 * @brief Transmits an OMP response packet.
 *
 * @param ss                    The streamer to transmit via.
 * @param buf                   Buffer containing the response packet.
 * @param arg                   Optional streamer argument.
 *
 * @return                      0 on success, MGMT_ERR_[...] code on failure.
 */
typedef void omp_tx_rsp_fn(struct mgmt_ctxt *ctxt, void *request, int retval);

/**
 * @brief Decodes, encodes, and transmits SMP packets.
 */
struct omp_streamer {
    struct mgmt_streamer mgmt_stmr;
    omp_tx_rsp_fn *tx_rsp_cb;
    struct CborEncoder rsp_encoder;
};

/**
 * @brief Processes a single OMP request packet and sends all corresponding
 *        responses.
 *
 * Processes all OMP requests in an incoming packet.  Requests are processed
 * sequentially from the start of the packet to the end.  Each response is sent
 * individually in its own packet.  If a request elicits an error response,
 * processing of the packet is aborted.  This function consumes the supplied
 * request buffer regardless of the outcome.
 *
 * @param streamer              The streamer providing the required OMP
 *                                  callbacks.
 * @param req                   The request packet to process.
 *
 * @return                      0 on success, MGMT_ERR_[...] code on failure.
 */
int omp_process_request_packet(struct omp_streamer *streamer, struct os_mbuf *m,
                               void *req);

#ifdef __cplusplus
}
#endif

#endif /* H_OMP_ */
