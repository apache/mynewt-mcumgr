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
 * @file cbor_port.c  (libmcu/cbor port)
 *
 * Implements the abstract mgmt_cbor_* interface using libmcu/cbor.
 *
 * Decoder design
 * --------------
 * libmcu/cbor parses the entire CBOR message eagerly into a flat
 * cbor_item_t[] array.  A static array s_items[] is used as the backing
 * store so that child decoder structs (obtained via enter_container) stay
 * small — they only hold pointers and indices, not the array itself.
 *
 * This assumes single-threaded access, which is true for all mcumgr
 * embedded deployments.
 *
 * Item traversal
 * --------------
 * In the flat items array:
 *   - Scalar items (integer, string, bool, null, float) occupy exactly one
 *     slot.  Advancing past them: item_idx++.
 *   - Container items (MAP, ARRAY) also occupy one slot.
 *     For ARRAY:  item.size = element count; advance = 1 + item.size.
 *     For MAP:    item.size = pair count;    advance = 1 + item.size * 2.
 *   - STRING items (byte and text): item.offset points to the first DATA
 *     byte, NOT the CBOR header byte.  Major type must be inferred from the
 *     header byte at item.offset - (following_bytes + 1).
 *
 * Container boundary: items[i]'s children live at
 *   [i + 1 .. i + 1 + items[i].size).
 */

#include <assert.h>
#include <string.h>

#include "cbor/cbor.h"
#include "cbor/encoder.h"
#include "cbor/decoder.h"
#include "cbor/parser.h"
#include "mgmt/cbor_port.h"

/* CBOR major types (upper 3 bits of the initial byte). */
#define CBOR_MAJOR_UINT         0u
#define CBOR_MAJOR_NEGINT       1u
#define CBOR_MAJOR_BYTES        2u
#define CBOR_MAJOR_TEXT         3u
#define CBOR_MAJOR_ARRAY        4u
#define CBOR_MAJOR_MAP          5u
#define CBOR_MAJOR_TAG          6u
#define CBOR_MAJOR_FLOAT_SIMPLE 7u

/* Additional info values for float / simple types. */
#define CBOR_AI_FALSE   20u
#define CBOR_AI_TRUE    21u
#define CBOR_AI_NULL    22u
#define CBOR_AI_HALF    25u
#define CBOR_AI_FLOAT   26u
#define CBOR_AI_DOUBLE  27u

/* MGMT_CBOR_MAX_ITEMS is defined in mgmt_cbor_port_impl.h */

/* =========================================================================
 * Helpers
 * ========================================================================= */

static inline uint8_t raw_byte(const mgmt_cbor_decoder_t *dec)
{
    return dec->msg[dec->items[dec->item_idx].offset];
}

static inline uint8_t major_type(uint8_t b) { return b >> 5; }
static inline uint8_t add_info(uint8_t b)   { return b & 0x1fu; }

/* =========================================================================
 * Encoder
 * ========================================================================= */

int
mgmt_cbor_encoder_init_buf(mgmt_cbor_encoder_t *enc, void *buf, size_t bufsize)
{
    cbor_writer_init(&enc->writer, buf, bufsize);
    enc->depth = 0;
    return MGMT_CBOR_OK;
}

size_t
mgmt_cbor_encoder_bytes_written(const mgmt_cbor_encoder_t *enc)
{
    return cbor_writer_len(&enc->writer);
}

int
mgmt_cbor_encode_uint(mgmt_cbor_encoder_t *enc, uint64_t value)
{
    cbor_error_t err = cbor_encode_unsigned_integer(&enc->writer, value);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_int(mgmt_cbor_encoder_t *enc, int64_t value)
{
    cbor_error_t err;
    if (value >= 0) {
        err = cbor_encode_unsigned_integer(&enc->writer, (uint64_t)value);
    } else {
        err = cbor_encode_negative_integer(&enc->writer, value);
    }
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_text(mgmt_cbor_encoder_t *enc, const char *str, size_t len)
{
    cbor_error_t err = cbor_encode_text_string(&enc->writer, str, len);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_text_z(mgmt_cbor_encoder_t *enc, const char *str)
{
    cbor_error_t err = cbor_encode_null_terminated_text_string(&enc->writer, str);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_bytes(mgmt_cbor_encoder_t *enc, const uint8_t *data, size_t len)
{
    cbor_error_t err = cbor_encode_byte_string(&enc->writer, data, len);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_bool(mgmt_cbor_encoder_t *enc, bool value)
{
    cbor_error_t err = cbor_encode_bool(&enc->writer, value);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_null(mgmt_cbor_encoder_t *enc)
{
    cbor_error_t err = cbor_encode_null(&enc->writer);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_float(mgmt_cbor_encoder_t *enc, float value)
{
    cbor_error_t err = cbor_encode_float(&enc->writer, value);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_double(mgmt_cbor_encoder_t *enc, double value)
{
    cbor_error_t err = cbor_encode_double(&enc->writer, value);
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_encode_half_float(mgmt_cbor_encoder_t *enc, const void *value)
{
    /*
     * libmcu/cbor does not expose a half-float encoder.
     * Encode as a 2-byte simple value in IEEE 754 half-float format.
     * The raw bytes come from the caller (tinycbor passes them as uint16_t*).
     */
    uint16_t raw;
    memcpy(&raw, value, sizeof(raw));
    /* CBOR half-float: major type 7 (0xe0), additional info 25 (0xf9) */
    uint8_t hdr = 0xf9u;
    uint8_t hi  = (uint8_t)(raw >> 8);
    uint8_t lo  = (uint8_t)(raw & 0xffu);
    cbor_writer_t *w = &enc->writer;
    /* Write manually since no API exists */
    if (w->bufidx + 3 > w->bufsize) {
        return MGMT_CBOR_ERR_NOMEM;
    }
    w->buf[w->bufidx++] = hdr;
    w->buf[w->bufidx++] = hi;
    w->buf[w->bufidx++] = lo;
    return MGMT_CBOR_OK;
}

int
mgmt_cbor_map_begin(mgmt_cbor_encoder_t *enc)
{
    cbor_error_t err = cbor_encode_map_indefinite(&enc->writer);
    if (err == CBOR_SUCCESS) {
        enc->depth++;
    }
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_map_end(mgmt_cbor_encoder_t *enc)
{
    assert(enc->depth > 0);
    cbor_error_t err = cbor_encode_break(&enc->writer);
    if (err == CBOR_SUCCESS) {
        enc->depth--;
    }
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_array_begin(mgmt_cbor_encoder_t *enc)
{
    cbor_error_t err = cbor_encode_array_indefinite(&enc->writer);
    if (err == CBOR_SUCCESS) {
        enc->depth++;
    }
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

int
mgmt_cbor_array_end(mgmt_cbor_encoder_t *enc)
{
    assert(enc->depth > 0);
    cbor_error_t err = cbor_encode_break(&enc->writer);
    if (err == CBOR_SUCCESS) {
        enc->depth--;
    }
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_NOMEM;
}

/* =========================================================================
 * Decoder
 * ========================================================================= */

int
mgmt_cbor_decoder_init(mgmt_cbor_decoder_t *dec, const void *buf, size_t len)
{
    cbor_reader_t reader;
    size_t nitems = 0;

    cbor_reader_init(&reader, dec->_storage, MGMT_CBOR_MAX_ITEMS);
    cbor_error_t err = cbor_parse(&reader, buf, len, &nitems);
    if (err != CBOR_SUCCESS && err != CBOR_OVERRUN) {
        return MGMT_CBOR_ERR_INVALID;
    }

    dec->msg      = (const uint8_t *)buf;
    dec->msglen   = len;
    dec->items    = dec->_storage;
    dec->nitems   = nitems;
    dec->item_idx = 0;
    dec->end_idx  = nitems;
    return MGMT_CBOR_OK;
}

bool
mgmt_cbor_dec_is_map(const mgmt_cbor_decoder_t *dec)
{
    if (dec->item_idx >= dec->end_idx) {
        return false;
    }
    return dec->items[dec->item_idx].type == CBOR_ITEM_MAP;
}

int
mgmt_cbor_dec_enter_container(mgmt_cbor_decoder_t *dec, mgmt_cbor_decoder_t *child)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    const cbor_item_t *item = &dec->items[dec->item_idx];
    if (item->type != CBOR_ITEM_MAP && item->type != CBOR_ITEM_ARRAY) {
        return MGMT_CBOR_ERR_INVALID;
    }

    child->msg      = dec->msg;
    child->msglen   = dec->msglen;
    child->items    = dec->items;   /* share root's items storage */
    child->nitems   = dec->nitems;
    child->item_idx = dec->item_idx + 1;

    /* item->size is the CBOR-encoded length:
     *   - MAP: pair count  → flat descendants = size * 2
     *   - ARRAY: item count → flat descendants = size
     * Indefinite containers (size == SIZE_MAX) use the parent boundary;
     * the BREAK item in the flat array terminates iteration naturally. */
    if (item->size == (size_t)CBOR_INDEFINITE_VALUE) {
        child->end_idx = dec->end_idx;
    } else if (item->type == CBOR_ITEM_MAP) {
        child->end_idx = dec->item_idx + 1u + item->size * 2u;
    } else {
        child->end_idx = dec->item_idx + 1u + item->size;
    }
    return MGMT_CBOR_OK;
}

int
mgmt_cbor_dec_leave_container(mgmt_cbor_decoder_t *dec, mgmt_cbor_decoder_t *child)
{
    /* Advance parent past the entire container. */
    dec->item_idx = child->end_idx;
    return MGMT_CBOR_OK;
}

bool
mgmt_cbor_dec_is_valid(const mgmt_cbor_decoder_t *dec)
{
    return dec->item_idx < dec->end_idx;
}

bool
mgmt_cbor_dec_at_end(const mgmt_cbor_decoder_t *dec)
{
    return dec->item_idx >= dec->end_idx;
}

int
mgmt_cbor_dec_type(const mgmt_cbor_decoder_t *dec)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_TYPE_INVALID;
    }
    const cbor_item_t *item = &dec->items[dec->item_idx];
    uint8_t b = dec->msg[item->offset];
    uint8_t mt = major_type(b);
    uint8_t ai = add_info(b);

    switch (item->type) {
    case CBOR_ITEM_INTEGER:
        return (mt == CBOR_MAJOR_UINT) ? MGMT_CBOR_TYPE_UINT : MGMT_CBOR_TYPE_INT;
    case CBOR_ITEM_STRING: {
        /* libmcu/cbor sets item->offset to the first DATA byte (after the
         * CBOR header).  Reading dec->msg[item->offset] gives a data byte,
         * not the header byte, so we cannot use `mt` here.  Reconstruct the
         * header byte position from item->size to get the real major type. */
        size_t fol = (item->size == (size_t)CBOR_INDEFINITE_VALUE) ? 0u
                   : (item->size < 24u)      ? 0u
                   : (item->size <= 0xffu)   ? 1u
                   : (item->size <= 0xffffu) ? 2u
                   :                           4u;
        uint8_t hdr = dec->msg[item->offset - fol - 1u];
        return (major_type(hdr) == CBOR_MAJOR_TEXT)
               ? MGMT_CBOR_TYPE_TEXT_STRING : MGMT_CBOR_TYPE_BYTE_STRING;
    }
    case CBOR_ITEM_ARRAY:
        return MGMT_CBOR_TYPE_ARRAY;
    case CBOR_ITEM_MAP:
        return MGMT_CBOR_TYPE_MAP;
    case CBOR_ITEM_FLOAT:
        if (ai == CBOR_AI_HALF)   return MGMT_CBOR_TYPE_HALF_FLOAT;
        if (ai == CBOR_AI_FLOAT)  return MGMT_CBOR_TYPE_FLOAT;
        if (ai == CBOR_AI_DOUBLE) return MGMT_CBOR_TYPE_DOUBLE;
        return MGMT_CBOR_TYPE_INVALID;
    case CBOR_ITEM_SIMPLE_VALUE:
        if (ai == CBOR_AI_FALSE || ai == CBOR_AI_TRUE) return MGMT_CBOR_TYPE_BOOL;
        if (ai == CBOR_AI_NULL)                         return MGMT_CBOR_TYPE_NULL;
        return MGMT_CBOR_TYPE_INVALID;
    default:
        return MGMT_CBOR_TYPE_INVALID;
    }
}

int
mgmt_cbor_dec_advance(mgmt_cbor_decoder_t *dec)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    const cbor_item_t *item = &dec->items[dec->item_idx];
    /* For containers, skip the header + all flat descendants.
     * MAP size is pair count; each pair occupies 2 flat items.
     * Indefinite containers fall back to the scope boundary. */
    if (item->type == CBOR_ITEM_MAP || item->type == CBOR_ITEM_ARRAY) {
        if (item->size == (size_t)CBOR_INDEFINITE_VALUE) {
            dec->item_idx = dec->end_idx;
        } else if (item->type == CBOR_ITEM_MAP) {
            dec->item_idx += 1u + item->size * 2u;
        } else {
            dec->item_idx += 1u + item->size;
        }
    } else {
        dec->item_idx++;
    }
    return MGMT_CBOR_OK;
}

int
mgmt_cbor_dec_bool(const mgmt_cbor_decoder_t *dec, bool *out)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    const cbor_item_t *item = &dec->items[dec->item_idx];
    uint8_t ai = add_info(dec->msg[item->offset]);
    if (ai == CBOR_AI_TRUE) {
        *out = true;
        return MGMT_CBOR_OK;
    }
    if (ai == CBOR_AI_FALSE) {
        *out = false;
        return MGMT_CBOR_OK;
    }
    return MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_int64(const mgmt_cbor_decoder_t *dec, int64_t *out)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }

    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    const cbor_item_t *item = &dec->items[dec->item_idx];
    uint8_t mt = major_type(dec->msg[item->offset]);
    uint64_t tmp = 0;

    cbor_error_t err = cbor_decode(&reader, item, &tmp, sizeof(tmp));
    if (err != CBOR_SUCCESS) {
        return MGMT_CBOR_ERR_INVALID;
    }
    if (mt == CBOR_MAJOR_NEGINT) {
        *out = -(int64_t)(tmp + 1u);
    } else {
        *out = (int64_t)tmp;
    }
    return MGMT_CBOR_OK;
}

int
mgmt_cbor_dec_uint64(const mgmt_cbor_decoder_t *dec, uint64_t *out)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }

    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    cbor_error_t err = cbor_decode(&reader, &dec->items[dec->item_idx],
                                    out, sizeof(*out));
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_float(const mgmt_cbor_decoder_t *dec, float *out)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }

    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    cbor_error_t err = cbor_decode(&reader, &dec->items[dec->item_idx],
                                    out, sizeof(*out));
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_double(const mgmt_cbor_decoder_t *dec, double *out)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }

    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    cbor_error_t err = cbor_decode(&reader, &dec->items[dec->item_idx],
                                    out, sizeof(*out));
    return err == CBOR_SUCCESS ? MGMT_CBOR_OK : MGMT_CBOR_ERR_INVALID;
}

int
mgmt_cbor_dec_half_float(const mgmt_cbor_decoder_t *dec, void *out)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    /*
     * Half-float: the 2 value bytes follow the header byte.
     * cbor_decode_pointer returns a pointer to the value bytes.
     */
    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    const void *ptr = cbor_decode_pointer(&reader, &dec->items[dec->item_idx]);
    if (!ptr) {
        return MGMT_CBOR_ERR_INVALID;
    }
    memcpy(out, ptr, 2);
    return MGMT_CBOR_OK;
}

int
mgmt_cbor_dec_string_len(const mgmt_cbor_decoder_t *dec, size_t *out_len)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    *out_len = dec->items[dec->item_idx].size;
    return MGMT_CBOR_OK;
}

int
mgmt_cbor_dec_text_string_copy(const mgmt_cbor_decoder_t *dec,
                                char *buf, size_t *inout_len)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    const cbor_item_t *item = &dec->items[dec->item_idx];

    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    const void *ptr = cbor_decode_pointer(&reader, item);
    if (!ptr) {
        return MGMT_CBOR_ERR_INVALID;
    }
    size_t avail = *inout_len - 1; /* reserve one byte for NUL */
    size_t copy_len = item->size < avail ? item->size : avail;
    memcpy(buf, ptr, copy_len);
    buf[copy_len] = '\0';
    *inout_len = item->size;
    return item->size <= avail ? MGMT_CBOR_OK : MGMT_CBOR_ERR_OVERFLOW;
}

int
mgmt_cbor_dec_byte_string_copy(const mgmt_cbor_decoder_t *dec,
                                uint8_t *buf, size_t *inout_len)
{
    if (dec->item_idx >= dec->end_idx) {
        return MGMT_CBOR_ERR_INVALID;
    }
    const cbor_item_t *item = &dec->items[dec->item_idx];

    cbor_reader_t reader;
    cbor_reader_init(&reader, dec->items,
                     dec->nitems);
    reader.msg     = dec->msg;
    reader.msgsize = dec->msglen;

    const void *ptr = cbor_decode_pointer(&reader, item);
    if (!ptr) {
        return MGMT_CBOR_ERR_INVALID;
    }
    size_t copy_len = item->size < *inout_len ? item->size : *inout_len;
    memcpy(buf, ptr, copy_len);
    *inout_len = copy_len;
    return item->size <= copy_len ? MGMT_CBOR_OK : MGMT_CBOR_ERR_OVERFLOW;
}
