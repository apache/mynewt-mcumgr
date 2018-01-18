/****************************************************************************
**
** Copyright (C) 2015 Intel Corporation
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

#ifndef CBOR_DEFS_H
#define CBOR_DEFS_H

#include <limits.h>
#include <stdlib.h>
#include <inttypes.h>

#ifndef SIZE_MAX
/* Some systems fail to define SIZE_MAX in <stdint.h>, even though C99 requires it...
 * Conversion from signed to unsigned is defined in 6.3.1.3 (Signed and unsigned integers) p2,
 * which says: "the value is converted by repeatedly adding or subtracting one more than the
 * maximum value that can be represented in the new type until the value is in the range of the
 * new type."
 * So -1 gets converted to size_t by adding SIZE_MAX + 1, which results in SIZE_MAX.
 */
#  define SIZE_MAX ((size_t)-1)
#endif

#ifndef CBOR_API
#  define CBOR_API
#endif
#ifndef CBOR_PRIVATE_API
#  define CBOR_PRIVATE_API
#endif
#ifndef CBOR_INLINE_API
#  if defined(__cplusplus)
#    define CBOR_INLINE inline
#    define CBOR_INLINE_API inline
#  else
#    define CBOR_INLINE_API static CBOR_INLINE
#    if defined(_MSC_VER)
#      define CBOR_INLINE __inline
#    elif defined(__GNUC__)
#      define CBOR_INLINE __inline__
#    elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#      define CBOR_INLINE inline
#    else
#      define CBOR_INLINE
#    endif
#  endif
#endif

typedef enum CborType {
    CborIntegerType     = 0x00,
    CborByteStringType  = 0x40,
    CborTextStringType  = 0x60,
    CborArrayType       = 0x80,
    CborMapType         = 0xa0,
    CborTagType         = 0xc0,
    CborSimpleType      = 0xe0,
    CborBooleanType     = 0xf5,
    CborNullType        = 0xf6,
    CborUndefinedType   = 0xf7,
    CborHalfFloatType   = 0xf9,
    CborFloatType       = 0xfa,
    CborDoubleType      = 0xfb,

    CborInvalidType     = 0xff              /* equivalent to the break byte, so it will never be used */
} CborType;

typedef uint64_t CborTag;

typedef enum CborKnownTags {
    CborDateTimeStringTag          = 0,        /* RFC 3339 format: YYYY-MM-DD hh:mm:ss+zzzz */
    CborUnixTime_tTag              = 1,
    CborPositiveBignumTag          = 2,
    CborNegativeBignumTag          = 3,
    CborDecimalTag                 = 4,
    CborBigfloatTag                = 5,
    CborExpectedBase64urlTag       = 21,
    CborExpectedBase64Tag          = 22,
    CborExpectedBase16Tag          = 23,
    CborUriTag                     = 32,
    CborBase64urlTag               = 33,
    CborBase64Tag                  = 34,
    CborRegularExpressionTag       = 35,
    CborMimeMessageTag             = 36,       /* RFC 2045-2047 */
    CborSignatureTag               = 55799
} CborKnownTags;

/* #define the constants so we can check with #ifdef */
#define CborDateTimeStringTag CborDateTimeStringTag
#define CborUnixTime_tTag CborUnixTime_tTag
#define CborPositiveBignumTag CborPositiveBignumTag
#define CborNegativeBignumTag CborNegativeBignumTag
#define CborDecimalTag CborDecimalTag
#define CborBigfloatTag CborBigfloatTag
#define CborCOSE_Encrypt0Tag CborCOSE_Encrypt0Tag
#define CborCOSE_Mac0Tag CborCOSE_Mac0Tag
#define CborCOSE_Sign1Tag CborCOSE_Sign1Tag
#define CborExpectedBase64urlTag CborExpectedBase64urlTag
#define CborExpectedBase64Tag CborExpectedBase64Tag
#define CborExpectedBase16Tag CborExpectedBase16Tag
#define CborEncodedCborTag CborEncodedCborTag
#define CborUrlTag CborUrlTag
#define CborBase64urlTag CborBase64urlTag
#define CborBase64Tag CborBase64Tag
#define CborRegularExpressionTag CborRegularExpressionTag
#define CborMimeMessageTag CborMimeMessageTag
#define CborCOSE_EncryptTag CborCOSE_EncryptTag
#define CborCOSE_MacTag CborCOSE_MacTag
#define CborCOSE_SignTag CborCOSE_SignTag
#define CborSignatureTag CborSignatureTag

/* Error API */

typedef enum CborError {
    CborNoError = 0,

    /* errors in all modes */
    CborUnknownError,
    CborErrorUnknownLength,         /* request for length in array, map, or string with indeterminate length */
    CborErrorAdvancePastEOF,
    CborErrorIO,

    /* parser errors streaming errors */
    CborErrorGarbageAtEnd = 256,
    CborErrorUnexpectedEOF,
    CborErrorUnexpectedBreak,
    CborErrorUnknownType,           /* can only heppen in major type 7 */
    CborErrorIllegalType,           /* type not allowed here */
    CborErrorIllegalNumber,
    CborErrorIllegalSimpleType,     /* types of value less than 32 encoded in two bytes */

    /* parser errors in strict mode parsing only */
    CborErrorUnknownSimpleType = 512,
    CborErrorUnknownTag,
    CborErrorInappropriateTagForType,
    CborErrorDuplicateObjectKeys,
    CborErrorInvalidUtf8TextString,
    CborErrorExcludedType,
    CborErrorExcludedValue,
    CborErrorImproperValue,
    CborErrorOverlongEncoding,
    CborErrorMapKeyNotString,
    CborErrorMapNotSorted,
    CborErrorMapKeysNotUnique,

    /* encoder errors */
    CborErrorTooManyItems = 768,
    CborErrorTooFewItems,

    /* internal implementation errors */
    CborErrorDataTooLarge = 1024,
    CborErrorNestingTooDeep,
    CborErrorUnsupportedType,

    /* errors in converting to JSON */
    CborErrorJsonObjectKeyIsAggregate,
    CborErrorJsonObjectKeyNotString,
    CborErrorJsonNotImplemented,

    CborErrorOutOfMemory = (int) (~0U / 2 + 1),
    CborErrorInternalError = (int) (~0U / 2)    /* INT_MAX on two's complement machines */
} CborError;

/* Parser Flags */

enum CborParserIteratorFlags
{
    CborIteratorFlag_IntegerValueTooLarge   = 0x01,
    CborIteratorFlag_NegativeInteger        = 0x02,
    CborIteratorFlag_IteratingStringChunks  = 0x02,
    CborIteratorFlag_UnknownLength          = 0x04,
    CborIteratorFlag_ContainerIsMap         = 0x20
};

enum CborValidationFlags {
    /* Bit mapping:
     *  bits 0-7 (8 bits):      canonical format
     *  bits 8-11 (4 bits):     canonical format & strict mode
     *  bits 12-20 (8 bits):    strict mode
     *  bits 21-31 (10 bits):   other
     */

    CborValidateShortestIntegrals           = 0x0001,
    CborValidateShortestFloatingPoint       = 0x0002,
    CborValidateShortestNumbers             = CborValidateShortestIntegrals | CborValidateShortestFloatingPoint,
    CborValidateNoIndeterminateLength       = 0x0100,
    CborValidateMapIsSorted                 = 0x0200 | CborValidateNoIndeterminateLength,

    CborValidateCanonicalFormat             = 0x0fff,

    CborValidateMapKeysAreUnique            = 0x1000 | CborValidateMapIsSorted,
    CborValidateTagUse                      = 0x2000,
    CborValidateUtf8                        = 0x4000,

    CborValidateStrictMode                  = 0xfff00,

    CborValidateMapKeysAreString            = 0x100000,
    CborValidateNoUndefined                 = 0x200000,
    CborValidateNoTags                      = 0x400000,
    CborValidateFiniteFloatingPoint         = 0x800000,
    /* unused                               = 0x1000000, */
    /* unused                               = 0x2000000, */

    CborValidateNoUnknownSimpleTypesSA      = 0x4000000,
    CborValidateNoUnknownSimpleTypes        = 0x8000000 | CborValidateNoUnknownSimpleTypesSA,
    CborValidateNoUnknownTagsSA             = 0x10000000,
    CborValidateNoUnknownTagsSR             = 0x20000000 | CborValidateNoUnknownTagsSA,
    CborValidateNoUnknownTags               = 0x40000000 | CborValidateNoUnknownTagsSR,

    CborValidateCompleteData                = (int)0x80000000,

    CborValidateStrictest                   = (int)~0U,
    CborValidateBasic                       = 0
};

static const size_t CborIndefiniteLength = SIZE_MAX;

/* The following API requires a hosted C implementation (uses FILE*) */
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__-0 == 1

enum CborPrettyFlags {
    CborPrettyNumericEncodingIndicators     = 0x01,
    CborPrettyTextualEncodingIndicators     = 0,

    CborPrettyIndicateIndetermineLength     = 0x02,
    CborPrettyIndicateOverlongNumbers       = 0x04,

    CborPrettyShowStringFragments           = 0x100,
    CborPrettyMergeStringFragments          = 0,

    CborPrettyDefaultFlags          = CborPrettyIndicateIndetermineLength
};

#endif /* __STDC_HOSTED__ check */

#endif
