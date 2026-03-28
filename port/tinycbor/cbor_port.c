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
 * @file cbor_port.c  (tinycbor port)
 *
 * Implements the abstract mgmt_cbor_* interface using tinycbor.
 */

#include <assert.h>
#include <string.h>

#include "mgmt/cbor_port.h"

/* =========================================================================
 * Encoder
 * ========================================================================= */

int
mgmt_cbor_encoder_init_buf(mgmt_cbor_encoder_t *enc, void *buf, size_t bufsize)
{
    cbor_buf_writer_init(&enc->buf_writer, (uint8_t *)buf, bufsize);
    cbor_encoder_init(&enc->stack[0], &enc->buf_writer.enc, 0);
    enc->depth = 0;
    return MGMT_CBOR_OK;
}

size_t
mgmt_cbor_encoder_bytes_written(const mgmt_cbor_encoder_t *enc)
{
    /* cbor_encode_bytes_written reads writer->bytes_written */
    return (size_t)cbor_encode_bytes_written(
        (struct CborEncoder *)(uintptr_t)&enc->stack[0]);
}

static inline struct CborEncoder *
current_enc(mgmt_cbor_encoder_t *enc)
{
    return &enc->stack[enc->depth];
}

int
mgmt_cbor_encode_uint(mgmt_cbor_encoder_t *enc, uint64_t value)
{
    CborError err = cbor_encode_uint(current_enc(enc), value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_int(mgmt_cbor_encoder_t *enc, int64_t value)
{
    CborError err = cbor_encode_int(current_enc(enc), value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_text(mgmt_cbor_encoder_t *enc, const char *str, size_t len)
{
    CborError err = cbor_encode_text_string(current_enc(enc), str, len);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_text_z(mgmt_cbor_encoder_t *enc, const char *str)
{
    CborError err = cbor_encode_text_stringz(current_enc(enc), str);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_bytes(mgmt_cbor_encoder_t *enc, const uint8_t *data, size_t len)
{
    CborError err = cbor_encode_byte_string(current_enc(enc), data, len);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_bool(mgmt_cbor_encoder_t *enc, bool value)
{
    CborError err = cbor_encode_boolean(current_enc(enc), value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_null(mgmt_cbor_encoder_t *enc)
{
    CborError err = cbor_encode_null(current_enc(enc));
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

#ifndef CBOR_NO_FLOATING_POINT
int
mgmt_cbor_encode_float(mgmt_cbor_encoder_t *enc, float value)
{
    CborError err = cbor_encode_float(current_enc(enc), value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_double(mgmt_cbor_encoder_t *enc, double value)
{
    CborError err = cbor_encode_double(current_enc(enc), value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_half_float(mgmt_cbor_encoder_t *enc, const void *value)
{
    CborError err = cbor_encode_half_float(current_enc(enc), value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}
#endif /* CBOR_NO_FLOATING_POINT */

int
mgmt_cbor_map_begin(mgmt_cbor_encoder_t *enc)
{
    assert(enc->depth < MGMT_CBOR_ENCODER_STACK_DEPTH - 1);
    struct CborEncoder *parent = &enc->stack[enc->depth];
    struct CborEncoder *child  = &enc->stack[enc->depth + 1];
    CborError err = cbor_encoder_create_map(parent, child, CborIndefiniteLength);
    if (err == CborNoError) {
        enc->depth++;
    }
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_map_end(mgmt_cbor_encoder_t *enc)
{
    assert(enc->depth > 0);
    CborError err = cbor_encoder_close_container(
        &enc->stack[enc->depth - 1], &enc->stack[enc->depth]);
    if (err == CborNoError) {
        enc->depth--;
    }
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_array_begin(mgmt_cbor_encoder_t *enc)
{
    assert(enc->depth < MGMT_CBOR_ENCODER_STACK_DEPTH - 1);
    struct CborEncoder *parent = &enc->stack[enc->depth];
    struct CborEncoder *child  = &enc->stack[enc->depth + 1];
    CborError err = cbor_encoder_create_array(parent, child, CborIndefiniteLength);
    if (err == CborNoError) {
        enc->depth++;
    }
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_array_end(mgmt_cbor_encoder_t *enc)
{
    assert(enc->depth > 0);
    CborError err = cbor_encoder_close_container(
        &enc->stack[enc->depth - 1], &enc->stack[enc->depth]);
    if (err == CborNoError) {
        enc->depth--;
    }
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

/* =========================================================================
 * Decoder
 * ========================================================================= */

int
mgmt_cbor_decoder_init(mgmt_cbor_decoder_t *dec, const void *buf, size_t len)
{
    cbor_buf_reader_init(&dec->buf_reader, (const uint8_t *)buf, len);
    CborError err = cbor_parser_init(&dec->buf_reader.r, 0,
                                     &dec->parser, &dec->value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

bool
mgmt_cbor_dec_is_map(const mgmt_cbor_decoder_t *dec)
{
    return cbor_value_is_map(&dec->value);
}

int
mgmt_cbor_dec_enter_container(mgmt_cbor_decoder_t *dec, mgmt_cbor_decoder_t *child)
{
    /*
     * child->value.parser is set by tinycbor to parent's value.parser (i.e.
     * &dec->parser).  That is safe because dec always outlives child on the
     * call stack.  child->parser and child->buf_reader are left uninitialised
     * intentionally — they are only used in root decoders.
     */
    CborError err = cbor_value_enter_container(&dec->value, &child->value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_leave_container(mgmt_cbor_decoder_t *dec, mgmt_cbor_decoder_t *child)
{
    CborError err = cbor_value_leave_container(&dec->value, &child->value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

bool
mgmt_cbor_dec_is_valid(const mgmt_cbor_decoder_t *dec)
{
    return cbor_value_is_valid(&dec->value);
}

bool
mgmt_cbor_dec_at_end(const mgmt_cbor_decoder_t *dec)
{
    return cbor_value_at_end(&dec->value);
}

int
mgmt_cbor_dec_type(const mgmt_cbor_decoder_t *dec)
{
    CborType ct = cbor_value_get_type(&dec->value);
    switch (ct) {
    case CborIntegerType:    return MGMT_CBOR_TYPE_INT;
    case CborByteStringType: return MGMT_CBOR_TYPE_BYTE_STRING;
    case CborTextStringType: return MGMT_CBOR_TYPE_TEXT_STRING;
    case CborArrayType:      return MGMT_CBOR_TYPE_ARRAY;
    case CborMapType:        return MGMT_CBOR_TYPE_MAP;
    case CborBooleanType:    return MGMT_CBOR_TYPE_BOOL;
    case CborNullType:       return MGMT_CBOR_TYPE_NULL;
    case CborHalfFloatType:  return MGMT_CBOR_TYPE_HALF_FLOAT;
    case CborFloatType:      return MGMT_CBOR_TYPE_FLOAT;
    case CborDoubleType:     return MGMT_CBOR_TYPE_DOUBLE;
    default:                 return MGMT_CBOR_TYPE_INVALID;
    }
}

int
mgmt_cbor_dec_advance(mgmt_cbor_decoder_t *dec)
{
    CborError err = cbor_value_advance(&dec->value);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_bool(const mgmt_cbor_decoder_t *dec, bool *out)
{
    CborError err = cbor_value_get_boolean(&dec->value, out);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_int64(const mgmt_cbor_decoder_t *dec, int64_t *out)
{
    CborError err = cbor_value_get_int64(&dec->value, out);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_uint64(const mgmt_cbor_decoder_t *dec, uint64_t *out)
{
    CborError err = cbor_value_get_uint64(&dec->value, out);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

#ifndef CBOR_NO_FLOATING_POINT
int
mgmt_cbor_dec_float(const mgmt_cbor_decoder_t *dec, float *out)
{
    CborError err = cbor_value_get_float(&dec->value, out);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_double(const mgmt_cbor_decoder_t *dec, double *out)
{
    CborError err = cbor_value_get_double(&dec->value, out);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_half_float(const mgmt_cbor_decoder_t *dec, void *out)
{
    CborError err = cbor_value_get_half_float(&dec->value, out);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}
#endif /* CBOR_NO_FLOATING_POINT */

int
mgmt_cbor_dec_string_len(const mgmt_cbor_decoder_t *dec, size_t *out_len)
{
    CborError err = cbor_value_calculate_string_length(&dec->value, out_len);
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_text_string_copy(const mgmt_cbor_decoder_t *dec,
                                char *buf, size_t *inout_len)
{
    /*
     * cbor_value_copy_text_string with next=NULL copies the string and does
     * NOT advance the iterator.  *inout_len is in/out: buf capacity in,
     * actual length out.  tinycbor does NOT add a NUL terminator when
     * next=NULL, so we do it manually.
     */
    size_t len = *inout_len - 1; /* reserve one byte for NUL */
    CborError err = cbor_value_copy_text_string(&dec->value, buf, &len, NULL);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
        return MGMT_CBOR_ERR_INVALID;
    }
    buf[len < *inout_len ? len : *inout_len - 1] = '\0';
    *inout_len = len;
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_OVERFLOW;
}

int
mgmt_cbor_dec_byte_string_copy(const mgmt_cbor_decoder_t *dec,
                                uint8_t *buf, size_t *inout_len)
{
    CborError err = cbor_value_copy_byte_string(&dec->value, buf, inout_len, NULL);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
        return MGMT_CBOR_ERR_INVALID;
    }
    return err == CborNoError ? MGMT_CBOR_OK : MGMT_CBOR_ERR_OVERFLOW;
}
