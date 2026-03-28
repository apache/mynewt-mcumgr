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
 * @file mgmt_cbor_port_impl.h  (libmcu/cbor port)
 *
 * Concrete struct definitions for mgmt_cbor_encoder_t and
 * mgmt_cbor_decoder_t when using the libmcu/cbor backend.
 *
 * Included automatically by mgmt/cbor_port.h when the build system
 * adds port/libmcu_cbor/ to the include search path.
 *
 * Design notes
 * ------------
 * Encoder: libmcu/cbor uses a simple cbor_writer_t (pointer + size + index).
 *   Indefinite-length containers are opened with cbor_encode_map_indefinite /
 *   cbor_encode_array_indefinite and closed with cbor_encode_break.  A depth
 *   counter tracks open containers for validation.
 *
 * Decoder: libmcu/cbor parses the entire message eagerly into a flat array of
 *   cbor_item_t descriptors.  The items array is allocated statically inside
 *   cbor_port.c and shared among all decoder instances (root + children).
 *   A root decoder holds msg/msglen pointers and the total item count; a child
 *   decoder obtained via mgmt_cbor_dec_enter_container() shares those
 *   pointers and limits iteration to [item_idx, end_idx).
 */

#ifndef H_MGMT_CBOR_PORT_IMPL_LIBMCU_CBOR_
#define H_MGMT_CBOR_PORT_IMPL_LIBMCU_CBOR_

#include "cbor/base.h"

/**
 * Encoder context.
 * depth tracks the number of open indefinite-length containers.
 */
struct mgmt_cbor_encoder {
    cbor_writer_t writer;
    int           depth;
};

/**
 * Maximum number of flat CBOR items the decoder can hold per request.
 * mcumgr incoming commands are small flat maps; 16 is sufficient for all
 * current command types.  Override by defining before including this header.
 */
#ifndef MGMT_CBOR_MAX_ITEMS
#define MGMT_CBOR_MAX_ITEMS  16
#endif

/**
 * Decoder context.
 *
 * msg / msglen — pointer to the raw CBOR bytes (not owned).
 * items        — pointer to the flat item array; root decoders point to
 *                _storage[], child decoders share the root's pointer.
 * nitems       — total parsed items (meaningful in root decoders only).
 * item_idx     — current position in items[].
 * end_idx      — exclusive upper bound for the current container scope.
 * _storage     — item backing store; only used by root decoders.
 *                Child decoders created by enter_container share the root's
 *                items pointer and never read this field.
 */
struct mgmt_cbor_decoder {
    const uint8_t *msg;
    size_t         msglen;
    cbor_item_t   *items;
    size_t         nitems;
    size_t         item_idx;
    size_t         end_idx;
    cbor_item_t    _storage[MGMT_CBOR_MAX_ITEMS];
};

#endif /* H_MGMT_CBOR_PORT_IMPL_LIBMCU_CBOR_ */
