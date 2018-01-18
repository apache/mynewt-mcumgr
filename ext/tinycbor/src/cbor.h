/****************************************************************************
**
** Copyright (C) 2017 Intel Corporation
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
****************************************************************************/

#ifndef CBOR_H
#define CBOR_H

#ifndef assert
#include <assert.h>
#endif
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cbor_buf_writer.h"
#include "cbor_buf_reader.h"
#include "cbor_defs.h"
#include "tinycbor-version.h"

#define TINYCBOR_VERSION            ((TINYCBOR_VERSION_MAJOR << 16) | (TINYCBOR_VERSION_MINOR << 8) | TINYCBOR_VERSION_PATCH)

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

CBOR_API const char *cbor_error_string(CborError error);

/* Encoder API */
struct CborEncoder
{
    cbor_encoder_writer *writer;
    void *writer_arg;
#ifndef NO_DFLT_WRITER
    struct cbor_buf_writer wr;
#endif
    size_t added;
    size_t container_size;
    int flags;
};

typedef struct CborEncoder CborEncoder;

#ifndef NO_DFLT_WRITER
CBOR_API void cbor_encoder_init(CborEncoder *encoder, uint8_t *buffer, size_t size, int flags);
#endif
CBOR_API void cbor_encoder_cust_writer_init(CborEncoder *encoder, struct cbor_encoder_writer *w, int flags);
CBOR_API CborError cbor_encode_uint(CborEncoder *encoder, uint64_t value);
CBOR_API CborError cbor_encode_int(CborEncoder *encoder, int64_t value);
CBOR_API CborError cbor_encode_negative_int(CborEncoder *encoder, uint64_t absolute_value);
CBOR_API CborError cbor_encode_simple_value(CborEncoder *encoder, uint8_t value);
CBOR_API CborError cbor_encode_tag(CborEncoder *encoder, CborTag tag);
CBOR_API CborError cbor_encode_text_string(CborEncoder *encoder, const char *string, size_t length);
CBOR_INLINE_API CborError cbor_encode_text_stringz(CborEncoder *encoder, const char *string)
{ return cbor_encode_text_string(encoder, string, strlen(string)); }
CBOR_API CborError cbor_encode_byte_string(CborEncoder *encoder, const uint8_t *string, size_t length);
CBOR_API CborError cbor_encode_floating_point(CborEncoder *encoder, CborType fpType, const void *value);
CBOR_INLINE_API int cbor_encode_bytes_written(CborEncoder *encoder)
{   return encoder->writer->bytes_written; }
CBOR_INLINE_API CborError cbor_encode_boolean(CborEncoder *encoder, bool value)
{ return cbor_encode_simple_value(encoder, (int)value - 1 + (CborBooleanType & 0x1f)); }
CBOR_INLINE_API CborError cbor_encode_null(CborEncoder *encoder)
{ return cbor_encode_simple_value(encoder, CborNullType & 0x1f); }
CBOR_INLINE_API CborError cbor_encode_undefined(CborEncoder *encoder)
{ return cbor_encode_simple_value(encoder, CborUndefinedType & 0x1f); }

CBOR_INLINE_API CborError cbor_encode_half_float(CborEncoder *encoder, const void *value)
{ return cbor_encode_floating_point(encoder, CborHalfFloatType, value); }
CBOR_INLINE_API CborError cbor_encode_float(CborEncoder *encoder, float value)
{ return cbor_encode_floating_point(encoder, CborFloatType, &value); }
CBOR_INLINE_API CborError cbor_encode_double(CborEncoder *encoder, double value)
{ return cbor_encode_floating_point(encoder, CborDoubleType, &value); }

CBOR_API CborError cbor_encoder_create_array(CborEncoder *encoder, CborEncoder *arrayEncoder, size_t length);
CBOR_API CborError cbor_encoder_create_map(CborEncoder *encoder, CborEncoder *mapEncoder, size_t length);
CBOR_API CborError cbor_encoder_close_container(CborEncoder *encoder, const CborEncoder *containerEncoder);
CBOR_API CborError cbor_encoder_close_container_checked(CborEncoder *encoder, const CborEncoder *containerEncoder);

/* Parser API */
struct CborParser
{
#ifndef NO_DFLT_READER
    struct cbor_buf_reader br;
#endif
    struct cbor_decoder_reader *d;
    int end;
    int flags;
};
typedef struct CborParser CborParser;

struct CborValue
{
    const CborParser *parser;
    int offset;
    uint32_t remaining;
    uint32_t remainingclen;
    uint16_t extra;
    uint8_t type;
    uint8_t flags;
};
typedef struct CborValue CborValue;

#ifndef NO_DFLT_READER
CBOR_API CborError cbor_parser_init(const uint8_t *buffer, size_t size, int flags, CborParser *parser, CborValue *it);
#endif
CBOR_API CborError cbor_parser_cust_reader_init(struct cbor_decoder_reader *r, int flags, CborParser *parser, CborValue *it);

CBOR_API CborError cbor_value_validate_basic(const CborValue *it);

CBOR_INLINE_API bool cbor_value_at_end(const CborValue *it)
{ return it->remaining == 0; }
CBOR_API CborError cbor_value_advance_fixed(CborValue *it);
CBOR_API CborError cbor_value_advance(CborValue *it);
CBOR_INLINE_API bool cbor_value_is_container(const CborValue *it)
{ return it->type == CborArrayType || it->type == CborMapType; }
CBOR_API CborError cbor_value_enter_container(const CborValue *it, CborValue *recursed);
CBOR_API CborError cbor_value_leave_container(CborValue *it, const CborValue *recursed);

CBOR_PRIVATE_API uint64_t _cbor_value_decode_int64_internal(const CborValue *value);
CBOR_INLINE_API uint64_t _cbor_value_extract_int64_helper(const CborValue *value)
{
    return value->flags & CborIteratorFlag_IntegerValueTooLarge ?
                _cbor_value_decode_int64_internal(value) : value->extra;
}

CBOR_INLINE_API bool cbor_value_is_valid(const CborValue *value)
{ return value && value->type != CborInvalidType; }
CBOR_INLINE_API CborType cbor_value_get_type(const CborValue *value)
{ return (CborType)value->type; }

/* Null & undefined type */
CBOR_INLINE_API bool cbor_value_is_null(const CborValue *value)
{ return value->type == CborNullType; }
CBOR_INLINE_API bool cbor_value_is_undefined(const CborValue *value)
{ return value->type == CborUndefinedType; }

/* Booleans */
CBOR_INLINE_API bool cbor_value_is_boolean(const CborValue *value)
{ return value->type == CborBooleanType; }
CBOR_INLINE_API CborError cbor_value_get_boolean(const CborValue *value, bool *result)
{
    assert(cbor_value_is_boolean(value));
    *result = !!value->extra;
    return CborNoError;
}

/* Simple types */
CBOR_INLINE_API bool cbor_value_is_simple_type(const CborValue *value)
{ return value->type == CborSimpleType; }
CBOR_INLINE_API CborError cbor_value_get_simple_type(const CborValue *value, uint8_t *result)
{
    assert(cbor_value_is_simple_type(value));
    *result = (uint8_t)value->extra;
    return CborNoError;
}

/* Integers */
CBOR_INLINE_API bool cbor_value_is_integer(const CborValue *value)
{ return value->type == CborIntegerType; }
CBOR_INLINE_API bool cbor_value_is_unsigned_integer(const CborValue *value)
{ return cbor_value_is_integer(value) && (value->flags & CborIteratorFlag_NegativeInteger) == 0; }
CBOR_INLINE_API bool cbor_value_is_negative_integer(const CborValue *value)
{ return cbor_value_is_integer(value) && (value->flags & CborIteratorFlag_NegativeInteger); }

CBOR_INLINE_API CborError cbor_value_get_raw_integer(const CborValue *value, uint64_t *result)
{
    assert(cbor_value_is_integer(value));
    *result = _cbor_value_extract_int64_helper(value);
    return CborNoError;
}

CBOR_INLINE_API CborError cbor_value_get_uint64(const CborValue *value, uint64_t *result)
{
    assert(cbor_value_is_unsigned_integer(value));
    *result = _cbor_value_extract_int64_helper(value);
    return CborNoError;
}

CBOR_INLINE_API CborError cbor_value_get_int64(const CborValue *value, int64_t *result)
{
    assert(cbor_value_is_integer(value));
    *result = (int64_t) _cbor_value_extract_int64_helper(value);
    if (value->flags & CborIteratorFlag_NegativeInteger)
        *result = -*result - 1;
    return CborNoError;
}

CBOR_INLINE_API CborError cbor_value_get_int(const CborValue *value, int *result)
{
    assert(cbor_value_is_integer(value));
    *result = (int) _cbor_value_extract_int64_helper(value);
    if (value->flags & CborIteratorFlag_NegativeInteger)
        *result = -*result - 1;
    return CborNoError;
}

CBOR_API CborError cbor_value_get_int64_checked(const CborValue *value, int64_t *result);
CBOR_API CborError cbor_value_get_int_checked(const CborValue *value, int *result);

CBOR_INLINE_API bool cbor_value_is_length_known(const CborValue *value)
{ return (value->flags & CborIteratorFlag_UnknownLength) == 0; }

/* Tags */
CBOR_INLINE_API bool cbor_value_is_tag(const CborValue *value)
{ return value->type == CborTagType; }
CBOR_INLINE_API CborError cbor_value_get_tag(const CborValue *value, CborTag *result)
{
    assert(cbor_value_is_tag(value));
    *result = _cbor_value_extract_int64_helper(value);
    return CborNoError;
}
CBOR_API CborError cbor_value_skip_tag(CborValue *it);

/* Strings */
CBOR_INLINE_API bool cbor_value_is_byte_string(const CborValue *value)
{ return value->type == CborByteStringType; }
CBOR_INLINE_API bool cbor_value_is_text_string(const CborValue *value)
{ return value->type == CborTextStringType; }

CBOR_INLINE_API CborError cbor_value_get_string_length(const CborValue *value, size_t *length)
{
    assert(cbor_value_is_byte_string(value) || cbor_value_is_text_string(value));
    if (!cbor_value_is_length_known(value))
        return CborErrorUnknownLength;
    uint64_t v = _cbor_value_extract_int64_helper(value);
    *length = (size_t)v;
    if (*length != v)
        return CborErrorDataTooLarge;
    return CborNoError;
}

CBOR_PRIVATE_API CborError _cbor_value_copy_string(const CborValue *value, void *buffer,
                                                   size_t *buflen, CborValue *next);
CBOR_PRIVATE_API CborError _cbor_value_dup_string(const CborValue *value, void **buffer,
                                                  size_t *buflen, CborValue *next);

CBOR_API CborError cbor_value_calculate_string_length(const CborValue *value, size_t *length);

CBOR_INLINE_API CborError cbor_value_copy_text_string(const CborValue *value, char *buffer,
                                                      size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_text_string(value));
    return _cbor_value_copy_string(value, buffer, buflen, next);
}
CBOR_INLINE_API CborError cbor_value_copy_byte_string(const CborValue *value, uint8_t *buffer,
                                                      size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_byte_string(value));
    return _cbor_value_copy_string(value, buffer, buflen, next);
}

CBOR_INLINE_API CborError cbor_value_dup_text_string(const CborValue *value, char **buffer,
                                                     size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_text_string(value));
    return _cbor_value_dup_string(value, (void **)buffer, buflen, next);
}
CBOR_INLINE_API CborError cbor_value_dup_byte_string(const CborValue *value, uint8_t **buffer,
                                                     size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_byte_string(value));
    return _cbor_value_dup_string(value, (void **)buffer, buflen, next);
}

CBOR_PRIVATE_API CborError _cbor_value_get_string_chunk(const CborValue *value, const void **bufferptr,
                                                        size_t *len, CborValue *next);
CBOR_INLINE_API CborError cbor_value_get_text_string_chunk(const CborValue *value, const char **bufferptr,
                                                           size_t *len, CborValue *next)
{
    assert(cbor_value_is_text_string(value));
    return _cbor_value_get_string_chunk(value, (const void **)bufferptr, len, next);
}
CBOR_INLINE_API CborError cbor_value_get_byte_string_chunk(const CborValue *value, const uint8_t **bufferptr,
                                                           size_t *len, CborValue *next)
{
    assert(cbor_value_is_byte_string(value));
    return _cbor_value_get_string_chunk(value, (const void **)bufferptr, len, next);
}

CBOR_API CborError cbor_value_text_string_equals(const CborValue *value, const char *string, bool *result);

/* Maps and arrays */
CBOR_INLINE_API bool cbor_value_is_array(const CborValue *value)
{ return value->type == CborArrayType; }
CBOR_INLINE_API bool cbor_value_is_map(const CborValue *value)
{ return value->type == CborMapType; }

CBOR_INLINE_API CborError cbor_value_get_array_length(const CborValue *value, size_t *length)
{
    assert(cbor_value_is_array(value));
    if (!cbor_value_is_length_known(value))
        return CborErrorUnknownLength;
    uint64_t v = _cbor_value_extract_int64_helper(value);
    *length = (size_t)v;
    if (*length != v)
        return CborErrorDataTooLarge;
    return CborNoError;
}

CBOR_INLINE_API CborError cbor_value_get_map_length(const CborValue *value, size_t *length)
{
    assert(cbor_value_is_map(value));
    if (!cbor_value_is_length_known(value))
        return CborErrorUnknownLength;
    uint64_t v = _cbor_value_extract_int64_helper(value);
    *length = (size_t)v;
    if (*length != v)
        return CborErrorDataTooLarge;
    return CborNoError;
}

CBOR_API CborError cbor_value_map_find_value(const CborValue *map, const char *string, CborValue *element);

/* Floating point */
CBOR_INLINE_API bool cbor_value_is_half_float(const CborValue *value)
{ return value->type == CborHalfFloatType; }
CBOR_API CborError cbor_value_get_half_float(const CborValue *value, void *result);

CBOR_INLINE_API bool cbor_value_is_float(const CborValue *value)
{ return value->type == CborFloatType; }
CBOR_INLINE_API CborError cbor_value_get_float(const CborValue *value, float *result)
{
    assert(cbor_value_is_float(value));
    assert(value->flags & CborIteratorFlag_IntegerValueTooLarge);
    uint32_t data = (uint32_t)_cbor_value_decode_int64_internal(value);
    memcpy(result, &data, sizeof(*result));
    return CborNoError;
}

CBOR_INLINE_API bool cbor_value_is_double(const CborValue *value)
{ return value->type == CborDoubleType; }
CBOR_INLINE_API CborError cbor_value_get_double(const CborValue *value, double *result)
{
    assert(cbor_value_is_double(value));
    assert(value->flags & CborIteratorFlag_IntegerValueTooLarge);
    uint64_t data = _cbor_value_decode_int64_internal(value);
    memcpy(result, &data, sizeof(*result));
    return CborNoError;
}

/* Validation API */
CBOR_API CborError cbor_value_validate(const CborValue *it, int flags);

/* Human-readable (dump) API */

typedef CborError (*CborStreamFunction)(void *token, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(printf, 2, 3)))
#endif
;

CBOR_API CborError cbor_value_to_pretty_stream(CborStreamFunction streamFunction, void *token, CborValue *value, int flags);

/* The following API requires a hosted C implementation (uses FILE*) */
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__-0 == 1
CBOR_API CborError cbor_value_to_pretty_advance_flags(FILE *out, CborValue *value, int flags);
CBOR_API CborError cbor_value_to_pretty_advance(FILE *out, CborValue *value);
CBOR_INLINE_API CborError cbor_value_to_pretty(FILE *out, const CborValue *value)
{
    CborValue copy = *value;
    return cbor_value_to_pretty_advance_flags(out, &copy, CborPrettyDefaultFlags);
}
#endif /* __STDC_HOSTED__ check */

#ifdef __cplusplus
}
#endif

#endif /* CBOR_H */
