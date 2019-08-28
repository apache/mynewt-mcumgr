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

#ifndef H_OMP_IMPL
#define H_OMP_IMPL

#include "mgmt/mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

int omp_encode_mgmt_hdr(struct CborEncoder *enc, struct mgmt_hdr hdr);
int omp_send_err_rsp(struct CborEncoder *enc,
                     const struct mgmt_hdr *hdr,
                     int mgmt_status);
int omp_read_hdr(struct CborValue *cv, struct mgmt_hdr *out_hdr);
int omp_process_mgmt_hdr(struct mgmt_hdr *req_hdr,
                         struct mgmt_hdr *rsp_hdr,
                         struct mgmt_ctxt *ctxt);

#ifdef __cplusplus
}
#endif

#endif /* H_OMP_IMPL */
