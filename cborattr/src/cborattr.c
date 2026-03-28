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

#include "cborattr/cborattr.h"
#include "mgmt/cbor_port.h"

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#ifdef CONFIG_MGMT_CBORATTR_MAX_SIZE
#define CBORATTR_MAX_SIZE CONFIG_MGMT_CBORATTR_MAX_SIZE
#else
#define CBORATTR_MAX_SIZE 512
#endif
#endif

#ifdef MYNEWT
#include "syscfg/syscfg.h"
#include "os/os_mbuf.h"
#define CBORATTR_MAX_SIZE MYNEWT_VAL(CBORATTR_MAX_SIZE)
#endif

/* Error code alias used throughout this file — maps to non-zero on failure. */
#define CBORATTR_ERR_ILLEGAL_TYPE  1
#define CBORATTR_ERR_TOO_LARGE     2

/* this maps a mgmt_cbor_type_t to a matching CborAttrType.  The mapping is
 * not one-to-one because of signedness of integers, hence the function. */
static int
valid_attr_type(int ct, CborAttrType at)
{
    switch (at) {
    case CborAttrIntegerType:
    case CborAttrUnsignedIntegerType:
        return (ct == MGMT_CBOR_TYPE_INT || ct == MGMT_CBOR_TYPE_UINT);
    case CborAttrByteStringType:
        return ct == MGMT_CBOR_TYPE_BYTE_STRING;
    case CborAttrTextStringType:
        return ct == MGMT_CBOR_TYPE_TEXT_STRING;
    case CborAttrBooleanType:
        return ct == MGMT_CBOR_TYPE_BOOL;
#if FLOAT_SUPPORT
    case CborAttrHalfFloatType:
        return ct == MGMT_CBOR_TYPE_HALF_FLOAT;
    case CborAttrFloatType:
        return ct == MGMT_CBOR_TYPE_FLOAT;
    case CborAttrDoubleType:
        return ct == MGMT_CBOR_TYPE_DOUBLE;
#endif
    case CborAttrArrayType:
        return ct == MGMT_CBOR_TYPE_ARRAY;
    case CborAttrObjectType:
        return ct == MGMT_CBOR_TYPE_MAP;
    case CborAttrNullType:
        return ct == MGMT_CBOR_TYPE_NULL;
    default:
        return 0;
    }
}

/* this function finds the pointer to the memory location to
 * write or read an attribute from the cbor_attr_t structure */
static char *
cbor_target_address(const struct cbor_attr_t *cursor,
                    const struct cbor_array_t *parent, int offset)
{
    char *targetaddr = NULL;

    if (parent == NULL || parent->element_type != CborAttrStructObjectType) {
        /* ordinary case - use the address in the cursor structure */
        switch (cursor->type) {
        case CborAttrNullType:
            targetaddr = NULL;
            break;
        case CborAttrIntegerType:
            targetaddr = (char *)&cursor->addr.integer[offset];
            break;
        case CborAttrUnsignedIntegerType:
            targetaddr = (char *)&cursor->addr.uinteger[offset];
            break;
#if FLOAT_SUPPORT
        case CborAttrHalfFloatType:
            targetaddr = (char *)&cursor->addr.halffloat[offset];
            break;
        case CborAttrFloatType:
            targetaddr = (char *)&cursor->addr.fval[offset];
            break;
        case CborAttrDoubleType:
            targetaddr = (char *)&cursor->addr.real[offset];
            break;
#endif
        case CborAttrByteStringType:
            targetaddr = (char *) cursor->addr.bytestring.data;
            break;
        case CborAttrTextStringType:
            targetaddr = cursor->addr.string;
            break;
        case CborAttrBooleanType:
            targetaddr = (char *)&cursor->addr.boolean[offset];
            break;
        default:
            targetaddr = NULL;
            break;
        }
    } else {
        /* tricky case - hacking a member in an array of structures */
        targetaddr =
            parent->arr.objects.base + (offset * parent->arr.objects.stride) +
            cursor->addr.offset;
    }
    return targetaddr;
}

static int
cbor_internal_read_object(mgmt_cbor_decoder_t *root_value,
                          const struct cbor_attr_t *attrs,
                          const struct cbor_array_t *parent,
                          int offset)
{
    const struct cbor_attr_t *cursor, *best_match;
    char attrbuf[CBORATTR_MAX_SIZE + 1];
    void *lptr;
    mgmt_cbor_decoder_t cur_value;
    int err = 0;
    size_t len = 0;
    int type = MGMT_CBOR_TYPE_INVALID;

    /* stuff fields with defaults in case they're omitted in the input */
    for (cursor = attrs; cursor->attribute != NULL; cursor++) {
        if (!cursor->nodefault) {
            lptr = cbor_target_address(cursor, parent, offset);
            if (lptr != NULL) {
                switch (cursor->type) {
                case CborAttrIntegerType:
                    memcpy(lptr, &cursor->dflt.integer, sizeof(long long int));
                    break;
                case CborAttrUnsignedIntegerType:
                    memcpy(lptr, &cursor->dflt.integer,
                           sizeof(long long unsigned int));
                    break;
                case CborAttrBooleanType:
                    memcpy(lptr, &cursor->dflt.boolean, sizeof(bool));
                    break;
#if FLOAT_SUPPORT
                case CborAttrHalfFloatType:
                    memcpy(lptr, &cursor->dflt.halffloat, sizeof(uint16_t));
                    break;
                case CborAttrFloatType:
                    memcpy(lptr, &cursor->dflt.fval, sizeof(float));
                    break;
                case CborAttrDoubleType:
                    memcpy(lptr, &cursor->dflt.real, sizeof(double));
                    break;
#endif
                default:
                    break;
                }
            }
        }
    }

    if (!mgmt_cbor_dec_is_map(root_value)) {
        return CBORATTR_ERR_ILLEGAL_TYPE;
    }

    err |= mgmt_cbor_dec_enter_container(root_value, &cur_value);
    if (err) {
        return err;
    }

    /* contains key value pairs */
    while (mgmt_cbor_dec_is_valid(&cur_value) && !err) {
        /* get the attribute */
        if (mgmt_cbor_dec_type(&cur_value) == MGMT_CBOR_TYPE_TEXT_STRING) {
            if (mgmt_cbor_dec_string_len(&cur_value, &len) == 0) {
                if (len > CBORATTR_MAX_SIZE) {
                    err |= CBORATTR_ERR_TOO_LARGE;
                    break;
                }
                size_t copy_len = len + 1;
                err |= mgmt_cbor_dec_text_string_copy(&cur_value, attrbuf,
                                                        &copy_len);
            }

            /* advance past key to value, then get value type */
            err |= mgmt_cbor_dec_advance(&cur_value);
            if (mgmt_cbor_dec_is_valid(&cur_value)) {
                type = mgmt_cbor_dec_type(&cur_value);
            } else {
                err |= CBORATTR_ERR_ILLEGAL_TYPE;
                break;
            }
        } else {
            attrbuf[0] = '\0';
            type = mgmt_cbor_dec_type(&cur_value);
        }

        /* find this attribute in our list */
        best_match = NULL;
        for (cursor = attrs; cursor->attribute != NULL; cursor++) {
            if (valid_attr_type(type, cursor->type)) {
                if (cursor->attribute == CBORATTR_ATTR_UNNAMED &&
                    attrbuf[0] == '\0') {
                    best_match = cursor;
                } else if (strlen(cursor->attribute) == len &&
                    !memcmp(cursor->attribute, attrbuf, len)) {
                    break;
                }
            }
        }
        if (!cursor->attribute && best_match) {
            cursor = best_match;
        }
        /* we found a match */
        if (cursor->attribute != NULL) {
            lptr = cbor_target_address(cursor, parent, offset);
            switch (cursor->type) {
            case CborAttrNullType:
                /* nothing to do */
                break;
            case CborAttrBooleanType:
                err |= mgmt_cbor_dec_bool(&cur_value, lptr);
                break;
            case CborAttrIntegerType:
                err |= mgmt_cbor_dec_int64(&cur_value, lptr);
                break;
            case CborAttrUnsignedIntegerType:
                err |= mgmt_cbor_dec_uint64(&cur_value, lptr);
                break;
#if FLOAT_SUPPORT
            case CborAttrHalfFloatType:
                err |= mgmt_cbor_dec_half_float(&cur_value, lptr);
                break;
            case CborAttrFloatType:
                err |= mgmt_cbor_dec_float(&cur_value, lptr);
                break;
            case CborAttrDoubleType:
                err |= mgmt_cbor_dec_double(&cur_value, lptr);
                break;
#endif
            case CborAttrByteStringType: {
                size_t slen = cursor->len;
                err |= mgmt_cbor_dec_byte_string_copy(&cur_value, lptr, &slen);
                *cursor->addr.bytestring.len = slen;
                break;
            }
            case CborAttrTextStringType: {
                size_t slen = cursor->len;
                err |= mgmt_cbor_dec_text_string_copy(&cur_value, lptr, &slen);
                break;
            }
            case CborAttrArrayType:
                err |= cbor_read_array(&cur_value, &cursor->addr.array);
                continue;
            case CborAttrObjectType:
                err |= cbor_internal_read_object(&cur_value, cursor->addr.obj,
                                                 NULL, 0);
                continue;
            default:
                err |= CBORATTR_ERR_ILLEGAL_TYPE;
            }
        }
        err |= mgmt_cbor_dec_advance(&cur_value);
    }
    if (!err) {
        /* that should be it for this container */
        err |= mgmt_cbor_dec_leave_container(root_value, &cur_value);
    }
    return err;
}

int
cbor_read_array(mgmt_cbor_decoder_t *value, const struct cbor_array_t *arr)
{
    int err = 0;
    mgmt_cbor_decoder_t elem;
    int off, arrcount;
    size_t len;
    void *lptr;
    char *tp;

    err = mgmt_cbor_dec_enter_container(value, &elem);
    if (err) {
        return err;
    }
    arrcount = 0;
    tp = arr->arr.strings.store;
    for (off = 0; off < arr->maxlen; off++) {
        switch (arr->element_type) {
        case CborAttrBooleanType:
            lptr = &arr->arr.booleans.store[off];
            err |= mgmt_cbor_dec_bool(&elem, lptr);
            break;
        case CborAttrIntegerType:
            lptr = &arr->arr.integers.store[off];
            err |= mgmt_cbor_dec_int64(&elem, lptr);
            break;
        case CborAttrUnsignedIntegerType:
            lptr = &arr->arr.uintegers.store[off];
            err |= mgmt_cbor_dec_uint64(&elem, lptr);
            break;
#if FLOAT_SUPPORT
        case CborAttrHalfFloatType:
            lptr = &arr->arr.halffloats.store[off];
            err |= mgmt_cbor_dec_half_float(&elem, lptr);
            break;
        case CborAttrFloatType:
        case CborAttrDoubleType:
            lptr = &arr->arr.reals.store[off];
            err |= mgmt_cbor_dec_double(&elem, lptr);
            break;
#endif
        case CborAttrTextStringType:
            len = (size_t)(arr->arr.strings.storelen - (tp - arr->arr.strings.store));
            err |= mgmt_cbor_dec_text_string_copy(&elem, tp, &len);
            arr->arr.strings.ptrs[off] = tp;
            tp += len + 1;
            break;
        case CborAttrStructObjectType:
            err |= cbor_internal_read_object(&elem, arr->arr.objects.subtype,
                                             arr, off);
            break;
        default:
            err |= CBORATTR_ERR_ILLEGAL_TYPE;
            break;
        }
        arrcount++;
        if (arr->element_type != CborAttrStructObjectType) {
            err |= mgmt_cbor_dec_advance(&elem);
        }
        if (!mgmt_cbor_dec_is_valid(&elem)) {
            break;
        }
    }
    if (arr->count) {
        *arr->count = arrcount;
    }
    while (!mgmt_cbor_dec_at_end(&elem)) {
        err |= CBORATTR_ERR_TOO_LARGE;
        mgmt_cbor_dec_advance(&elem);
    }
    err |= mgmt_cbor_dec_leave_container(value, &elem);
    return err;
}

int
cbor_read_object(mgmt_cbor_decoder_t *value, const struct cbor_attr_t *attrs)
{
    return cbor_internal_read_object(value, attrs, NULL, 0);
}

/*
 * Read in cbor key/values from flat buffer pointed by data, and fill them
 * into attrs.
 *
 * @param data      Pointer to beginning of cbor encoded data
 * @param len       Number of bytes in the buffer
 * @param attrs     Array of cbor objects to look for.
 *
 * @return          0 on success; non-zero on failure.
 */
int
cbor_read_flat_attrs(const uint8_t *data, int len,
                     const struct cbor_attr_t *attrs)
{
    mgmt_cbor_decoder_t dec;
    int err;

    err = mgmt_cbor_decoder_init(&dec, data, (size_t)len);
    if (err != MGMT_CBOR_OK) {
        return -1;
    }
    return cbor_read_object(&dec, attrs);
}

#ifdef MYNEWT
static int cbor_write_val(mgmt_cbor_encoder_t *enc,
                          const struct cbor_out_val_t *val);

/*
 * Read in cbor key/values from os_mbuf pointed by m, and fill them
 * into attrs.
 *
 * @param m         Pointer to os_mbuf containing cbor encoded data
 * @param off       Offset into mbuf where cbor data begins
 * @param len       Number of bytes to decode
 * @param attrs     Array of cbor objects to look for.
 *
 * @return          0 on success; non-zero on failure.
 */
int
cbor_read_mbuf_attrs(struct os_mbuf *m, uint16_t off, uint16_t len,
                     const struct cbor_attr_t *attrs)
{
    /*
     * Copy mbuf data into a temporary buffer, then use the flat-buffer
     * decoder.  This avoids carrying the mbuf reader abstraction into the
     * new CBOR port layer.
     */
    uint8_t tmp[512];
    if (len > sizeof(tmp)) {
        return -1;
    }
    if (os_mbuf_copydata(m, off, len, tmp) != 0) {
        return -1;
    }
    return cbor_read_flat_attrs(tmp, len, attrs);
}

static int
cbor_write_arr_val(mgmt_cbor_encoder_t *enc,
                   const struct cbor_out_arr_val_t *arr)
{
    size_t i;
    int rc;

    rc = mgmt_cbor_array_begin(enc);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    for (i = 0; i < arr->len; i++) {
        rc = cbor_write_val(enc, &arr->elems[i]);
        if (rc != 0) {
            return SYS_ENOMEM;
        }
    }

    rc = mgmt_cbor_array_end(enc);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    return 0;
}

static int
cbor_write_val(mgmt_cbor_encoder_t *enc, const struct cbor_out_val_t *val)
{
    int len;
    int rc;

    switch (val->type) {
    case CborAttrNullType:
        rc = mgmt_cbor_encode_null(enc);
        break;

    case CborAttrBooleanType:
        rc = mgmt_cbor_encode_bool(enc, val->boolean);
        break;

    case CborAttrIntegerType:
        rc = mgmt_cbor_encode_int(enc, val->integer);
        break;

    case CborAttrUnsignedIntegerType:
        rc = mgmt_cbor_encode_uint(enc, val->uinteger);
        break;

#if FLOAT_SUPPORT
    case CborAttrHalfFloatType:
        rc = mgmt_cbor_encode_half_float(enc, &val->halffloat);
        break;

    case CborAttrFloatType:
        rc = mgmt_cbor_encode_float(enc, val->fval);
        break;

    case CborAttrDoubleType:
        rc = mgmt_cbor_encode_double(enc, val->real);
        break;
#endif

    case CborAttrByteStringType:
        if (val->bytestring.data == NULL &&
            val->bytestring.len != 0) {
            return SYS_EINVAL;
        }
        rc = mgmt_cbor_encode_bytes(enc, val->bytestring.data,
                                     val->bytestring.len);
        break;

    case CborAttrTextStringType:
        if (val->string == NULL) {
            len = 0;
        } else {
            len = (int)strlen(val->string);
        }
        rc = mgmt_cbor_encode_text(enc, val->string, (size_t)len);
        break;

    case CborAttrObjectType:
        rc = cbor_write_object(enc, val->obj);
        break;

    case CborAttrArrayType:
        rc = cbor_write_arr_val(enc, &val->array);
        break;

    default:
        return SYS_ENOTSUP;
    }

    if (rc != 0) {
        return SYS_ENOMEM;
    }

    return 0;
}

static int
cbor_write_attr(mgmt_cbor_encoder_t *enc, const struct cbor_out_attr_t *attr)
{
    int rc;

    if (attr->omit) {
        return 0;
    }

    if (!attr->attribute) {
        return SYS_EINVAL;
    }

    rc = mgmt_cbor_encode_text_z(enc, attr->attribute);
    if (rc != 0) {
        return rc;
    }

    rc = cbor_write_val(enc, &attr->val);
    return rc;
}

int
cbor_write_object(mgmt_cbor_encoder_t *enc, const struct cbor_out_attr_t *attrs)
{
    const struct cbor_out_attr_t *attr;
    int rc;

    rc = mgmt_cbor_map_begin(enc);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    for (attr = attrs; attr->val.type != 0; attr++) {
        rc = cbor_write_attr(enc, attr);
        if (rc != 0) {
            return rc;
        }
    }

    rc = mgmt_cbor_map_end(enc);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    return 0;
}

int
cbor_write_object_msys(const struct cbor_out_attr_t *attrs,
                       struct os_mbuf **out_om)
{
    uint8_t buf[512];
    mgmt_cbor_encoder_t enc;
    int rc;

    *out_om = os_msys_get_pkthdr(0, 0);
    if (*out_om == NULL) {
        return SYS_ENOMEM;
    }

    rc = mgmt_cbor_encoder_init_buf(&enc, buf, sizeof(buf));
    if (rc != 0) {
        os_mbuf_free_chain(*out_om);
        *out_om = NULL;
        return SYS_ENOMEM;
    }

    rc = cbor_write_object(&enc, attrs);
    if (rc != 0) {
        os_mbuf_free_chain(*out_om);
        *out_om = NULL;
        return rc;
    }

    size_t written = mgmt_cbor_encoder_bytes_written(&enc);
    if (os_mbuf_append(*out_om, buf, written) != 0) {
        os_mbuf_free_chain(*out_om);
        *out_om = NULL;
        return SYS_ENOMEM;
    }

    return 0;
}
#endif
