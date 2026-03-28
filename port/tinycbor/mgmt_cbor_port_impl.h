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
 * @file mgmt_cbor_port_impl.h  (tinycbor port)
 *
 * Concrete struct definitions for mgmt_cbor_encoder_t and
 * mgmt_cbor_decoder_t when using the tinycbor backend.
 *
 * Included automatically by mgmt/cbor_port.h when the build system
 * adds port/tinycbor/ to the include search path.
 */

#ifndef H_MGMT_CBOR_PORT_IMPL_TINYCBOR_
#define H_MGMT_CBOR_PORT_IMPL_TINYCBOR_

#include "tinycbor/cbor.h"
#include "tinycbor/cbor_buf_reader.h"
#include "tinycbor/cbor_buf_writer.h"

/**
 * Maximum nesting depth for indefinite-length containers.
 * A depth of 8 accommodates all realistic mcumgr payloads.
 */
#define MGMT_CBOR_ENCODER_STACK_DEPTH  8

/**
 * Encoder context.
 *
 * stack[0]    — root encoder (initialised with the output buffer writer).
 * stack[1..n] — one entry per open nested container; written by map_begin /
 *               array_begin and consumed by the matching _end.
 * depth       — current nesting level; 0 means no open container.
 */
struct mgmt_cbor_encoder {
    struct cbor_buf_writer buf_writer;  /* wraps the output buffer */
    struct CborEncoder     stack[MGMT_CBOR_ENCODER_STACK_DEPTH];
    int                    depth;
};

/**
 * Decoder context.
 *
 * buf_reader  — wraps the raw input buffer for cbor_parser_init.
 * parser      — tinycbor parser; lives here so it is always reachable.
 * value       — current iterator position.  value.parser always points
 *               back to &parser (for the root decoder) or to the parent's
 *               parser (for child decoders obtained via enter_container).
 *               Both are safe because the parent decoder always outlives
 *               its children on the call stack.
 */
struct mgmt_cbor_decoder {
    struct cbor_buf_reader buf_reader;
    struct CborParser      parser;
    struct CborValue       value;
};

#endif /* H_MGMT_CBOR_PORT_IMPL_TINYCBOR_ */
