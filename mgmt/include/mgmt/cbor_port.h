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
 * @file cbor_port.h
 * @brief Abstract CBOR encoder/decoder interface for mcumgr.
 *
 * Each CBOR backend (tinycbor, libmcu/cbor, …) provides:
 *   - A header named "mgmt_cbor_port_impl.h" with the concrete
 *     struct definitions for mgmt_cbor_encoder and mgmt_cbor_decoder.
 *   - A corresponding cbor_port.c that implements every function
 *     declared here.
 *
 * The build system selects the backend by adding the appropriate port
 * directory to the include path so that "mgmt_cbor_port_impl.h" resolves
 * to the right file.
 */

#ifndef H_MGMT_CBOR_PORT_
#define H_MGMT_CBOR_PORT_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Pull in the port-specific struct bodies.
 * Found via -I<port_dir> in the build system.
 */
#include "mgmt_cbor_port_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mgmt_cbor_encoder mgmt_cbor_encoder_t;
typedef struct mgmt_cbor_decoder mgmt_cbor_decoder_t;

typedef enum {
    MGMT_CBOR_OK           =  0,
    MGMT_CBOR_ERR_NOMEM    = -1,
    MGMT_CBOR_ERR_INVALID  = -2,
    MGMT_CBOR_ERR_OVERFLOW = -3,
} mgmt_cbor_err_t;

typedef enum {
    MGMT_CBOR_TYPE_UINT        = 0,
    MGMT_CBOR_TYPE_INT         = 1,
    MGMT_CBOR_TYPE_BYTE_STRING = 2,
    MGMT_CBOR_TYPE_TEXT_STRING = 3,
    MGMT_CBOR_TYPE_ARRAY       = 4,
    MGMT_CBOR_TYPE_MAP         = 5,
    MGMT_CBOR_TYPE_BOOL        = 6,
    MGMT_CBOR_TYPE_NULL        = 7,
    MGMT_CBOR_TYPE_HALF_FLOAT  = 8,
    MGMT_CBOR_TYPE_FLOAT       = 9,
    MGMT_CBOR_TYPE_DOUBLE      = 10,
    MGMT_CBOR_TYPE_INVALID     = -1,
} mgmt_cbor_type_t;

/* -------------------------------------------------------------------------
 * Encoder API
 * ------------------------------------------------------------------------- */

/** Initialise encoder to write into a fixed-size buffer. */
int    mgmt_cbor_encoder_init_buf(mgmt_cbor_encoder_t *enc,
                                   void *buf, size_t bufsize);

/** Return total bytes written by the encoder so far. */
size_t mgmt_cbor_encoder_bytes_written(const mgmt_cbor_encoder_t *enc);

/* Scalar encoding */
int    mgmt_cbor_encode_uint(mgmt_cbor_encoder_t *enc, uint64_t value);
int    mgmt_cbor_encode_int(mgmt_cbor_encoder_t *enc, int64_t value);
int    mgmt_cbor_encode_text(mgmt_cbor_encoder_t *enc,
                              const char *str, size_t len);
int    mgmt_cbor_encode_text_z(mgmt_cbor_encoder_t *enc, const char *str);
int    mgmt_cbor_encode_bytes(mgmt_cbor_encoder_t *enc,
                               const uint8_t *data, size_t len);
int    mgmt_cbor_encode_bool(mgmt_cbor_encoder_t *enc, bool value);
int    mgmt_cbor_encode_null(mgmt_cbor_encoder_t *enc);
#ifndef CBOR_NO_FLOATING_POINT
int    mgmt_cbor_encode_float(mgmt_cbor_encoder_t *enc, float value);
int    mgmt_cbor_encode_double(mgmt_cbor_encoder_t *enc, double value);
int    mgmt_cbor_encode_half_float(mgmt_cbor_encoder_t *enc, const void *value);
#endif /* CBOR_NO_FLOATING_POINT */

/**
 * Container encoding — indefinite-length maps and arrays.
 * Each begin() must be paired with a matching end().
 * An internal nesting stack eliminates the need to pass a child encoder.
 */
int    mgmt_cbor_map_begin(mgmt_cbor_encoder_t *enc);
int    mgmt_cbor_map_end(mgmt_cbor_encoder_t *enc);
int    mgmt_cbor_array_begin(mgmt_cbor_encoder_t *enc);
int    mgmt_cbor_array_end(mgmt_cbor_encoder_t *enc);

/* -------------------------------------------------------------------------
 * Decoder API
 * ------------------------------------------------------------------------- */

/**
 * Initialise the decoder from a raw CBOR buffer.
 * For pre-parse backends (libmcu/cbor) the entire message is parsed here.
 * For streaming backends (tinycbor) the parser is set up for lazy decoding.
 */
int    mgmt_cbor_decoder_init(mgmt_cbor_decoder_t *dec,
                               const void *buf, size_t len);

/** Return true if the current item is a CBOR map. */
bool   mgmt_cbor_dec_is_map(const mgmt_cbor_decoder_t *dec);

/**
 * Enter the map/array at the current position.
 * After this call *child* points to the first element of the container.
 * *dec* is not advanced; call mgmt_cbor_dec_leave_container() to move past
 * the container when done.
 */
int    mgmt_cbor_dec_enter_container(mgmt_cbor_decoder_t *dec,
                                      mgmt_cbor_decoder_t *child);

/**
 * Leave a container.
 * After this call *dec* is advanced past the container that was *child*.
 */
int    mgmt_cbor_dec_leave_container(mgmt_cbor_decoder_t *dec,
                                      mgmt_cbor_decoder_t *child);

/** Return true if *dec* points to a valid (non-end) item. */
bool   mgmt_cbor_dec_is_valid(const mgmt_cbor_decoder_t *dec);

/** Return true if *dec* is at the end of its container. */
bool   mgmt_cbor_dec_at_end(const mgmt_cbor_decoder_t *dec);

/** Return the mgmt_cbor_type_t of the current item. */
int    mgmt_cbor_dec_type(const mgmt_cbor_decoder_t *dec);

/** Advance to the next sibling item. */
int    mgmt_cbor_dec_advance(mgmt_cbor_decoder_t *dec);

/* Value extraction — dec must point to an item of matching type. */
int    mgmt_cbor_dec_bool(const mgmt_cbor_decoder_t *dec, bool *out);
int    mgmt_cbor_dec_int64(const mgmt_cbor_decoder_t *dec, int64_t *out);
int    mgmt_cbor_dec_uint64(const mgmt_cbor_decoder_t *dec, uint64_t *out);
#ifndef CBOR_NO_FLOATING_POINT
int    mgmt_cbor_dec_float(const mgmt_cbor_decoder_t *dec, float *out);
int    mgmt_cbor_dec_double(const mgmt_cbor_decoder_t *dec, double *out);
int    mgmt_cbor_dec_half_float(const mgmt_cbor_decoder_t *dec, void *out);
#endif /* CBOR_NO_FLOATING_POINT */

/**
 * Return the byte length of the current string item (text or byte string).
 * Does not advance dec.
 */
int    mgmt_cbor_dec_string_len(const mgmt_cbor_decoder_t *dec, size_t *out_len);

/**
 * Copy the current text-string value into buf.
 * *inout_len: in = available buf space (including NUL), out = string length.
 * The result is NUL-terminated.  Does not advance dec.
 */
int    mgmt_cbor_dec_text_string_copy(const mgmt_cbor_decoder_t *dec,
                                       char *buf, size_t *inout_len);

/**
 * Copy the current byte-string value into buf.
 * *inout_len: in = available buf space, out = number of bytes copied.
 * Does not advance dec.
 */
int    mgmt_cbor_dec_byte_string_copy(const mgmt_cbor_decoder_t *dec,
                                       uint8_t *buf, size_t *inout_len);

#ifdef __cplusplus
}
#endif

#endif /* H_MGMT_CBOR_PORT_ */
