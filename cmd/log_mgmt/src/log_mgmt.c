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
#include <stdio.h>

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "tinycbor/cbor_cnt_writer.h"
#include "log_mgmt/log_mgmt.h"
#include "log_mgmt/log_mgmt_impl.h"
#include "log_mgmt_config.h"
#include "log/log.h"

/** Context used during walks. */
struct log_walk_ctxt {
    /* The number of bytes encoded to the response so far. */
    size_t rsp_len;
    /* The encoder to use to write the current log entry. */
    struct CborEncoder *enc;
    /* Counter per encoder to understand if we are encoding the first chunk */
    uint32_t counter;
};

static mgmt_handler_fn log_mgmt_show;
static mgmt_handler_fn log_mgmt_clear;
static mgmt_handler_fn log_mgmt_module_list;
static mgmt_handler_fn log_mgmt_level_list;
static mgmt_handler_fn log_mgmt_logs_list;

static struct mgmt_handler log_mgmt_handlers[] = {
    [LOG_MGMT_ID_SHOW] =        { log_mgmt_show, NULL },
    [LOG_MGMT_ID_CLEAR] =       { NULL, log_mgmt_clear },
    [LOG_MGMT_ID_MODULE_LIST] = { log_mgmt_module_list, NULL },
    [LOG_MGMT_ID_LEVEL_LIST] =  { log_mgmt_level_list, NULL },
    [LOG_MGMT_ID_LOGS_LIST] =   { log_mgmt_logs_list, NULL },
};

#define LOG_MGMT_HANDLER_CNT \
    sizeof log_mgmt_handlers / sizeof log_mgmt_handlers[0]

static struct mgmt_group log_mgmt_group = {
    .mg_handlers = log_mgmt_handlers,
    .mg_handlers_count = LOG_MGMT_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_LOG,
};

static int
log_mgmt_encode_entry(CborEncoder *enc, const struct log_mgmt_entry *entry,
                      size_t *out_len)
{
    CborError err = CborNoError;
    CborEncoder rsp;
    CborEncoder str_encoder;
    uint16_t len;
    int off;

    len = cbor_encode_bytes_written(enc);

    err |= cbor_encoder_create_map(enc, &rsp, CborIndefiniteLength);
#if MYNEWT_VAL(LOG_VERSION) > 2
    switch (entry->type) {
    case LOG_MGMT_ETYPE_CBOR:
        err |= cbor_encode_text_stringz(&rsp, "type");
        err |= cbor_encode_text_stringz(&rsp, "cbor");
        break;
    case LOG_MGMT_ETYPE_BINARY:
        err |= cbor_encode_text_stringz(&rsp, "type");
        err |= cbor_encode_text_stringz(&rsp, "bin");
        break;
    case LOG_MGMT_ETYPE_STRING:
        err |= cbor_encode_text_stringz(&rsp, "type");
        err |= cbor_encode_text_stringz(&rsp, "str");
        break;
    default:
        cbor_encoder_close_container(&rsp, &str_encoder);
        return MGMT_ERR_ECORRUPT;
    }

    err |= cbor_encode_text_stringz(&rsp, "msg");

    /*
     * Write entry data as byte string. Since this may not fit into single
     * chunk of data we will write as indefinite-length byte string which is
     * basically a indefinite-length container with definite-length strings
     * inside.
     */
    err |= cbor_encoder_create_indef_byte_string(&rsp, &str_encoder);
    for (off = 0; off < entry->len && !err; ) {
        err |= cbor_encode_byte_string(&str_encoder, entry->data, entry->len);
        off += entry->len;
    }

    err |= cbor_encoder_close_container(&rsp, &str_encoder);
#else
    err |= cbor_encode_text_stringz(&rsp, "msg");
    err |= cbor_encode_text_stringz(&rsp, (char *)data);
#endif
    err |= cbor_encode_text_stringz(&rsp, "ts");
    err |= cbor_encode_int(&rsp, entry->ts);
    err |= cbor_encode_text_stringz(&rsp, "level");
    err |= cbor_encode_uint(&rsp, entry->level);
    err |= cbor_encode_text_stringz(&rsp, "index");
    err |= cbor_encode_uint(&rsp, entry->index);
    err |= cbor_encode_text_stringz(&rsp, "module");
    err |= cbor_encode_uint(&rsp, entry->module);
    if (entry->flags & LOG_MGMT_FLAGS_IMG_HASH) {
        err |= cbor_encode_text_stringz(&rsp, "imghash");
        err |= cbor_encode_byte_string(&rsp, entry->imghash,
                                       LOG_MGMT_IMG_HASHLEN);
    }

    err |= cbor_encoder_close_container(enc, &rsp);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    if (out_len != NULL) {
        *out_len = cbor_encode_bytes_written(enc) - len;
    }

    return MGMT_ERR_EOK;
}

static int
log_mgmt_cb_encode(struct log_mgmt_entry *entry, void *arg)
{
    struct CborCntWriter cnt_writer;
    struct log_walk_ctxt *ctxt;
    CborEncoder cnt_encoder;
    size_t entry_len;
    int rc;

    ctxt = arg;

    /*** First, determine if this entry would fit. */

    cbor_cnt_writer_init(&cnt_writer);
#ifdef __ZEPHYR__
    cbor_encoder_cust_writer_init(&cnt_encoder, &cnt_writer.enc, 0);
#else
    cbor_encoder_init(&cnt_encoder, &cnt_writer.enc, 0);
#endif
    rc = log_mgmt_encode_entry(&cnt_encoder, entry, &entry_len);
    if (rc != 0) {
        return rc;
    }

    /*
     * Check if the response is too long. If more than one entry is in the
     * response we will not add the current one and will return ENOMEM. If this
     * is just a single entry we add the generic too long message text.
     */
    /* `+ 1` to account for the CBOR array terminator. */
    if (ctxt->rsp_len + entry_len + 1 > LOG_MGMT_MAX_RSP_LEN) {
        /*
         * Is this just a single entry? If so, encode the generic error
         * message in the "msg" field of the response
         */
        if (ctxt->counter == 0) {
#if MYNEWT_VAL(LOG_VERSION) > 2
            entry->type = LOG_ETYPE_STRING;
#endif
            snprintf((char *)entry->data, LOG_MGMT_MAX_RSP_LEN,
                     "error: entry too large (%d bytes)", entry_len);
        }

        return MGMT_ERR_EMSGSIZE;
    }
    ctxt->rsp_len += entry_len;

    /*** The entry fits. Now encode it. */

    rc = log_mgmt_encode_entry(ctxt->enc, entry, NULL);
    if (rc != 0) {
        return rc;
    }

    ctxt->counter++;

    return 0;
}

static int
log_encode_entries(const struct log_mgmt_log *log, CborEncoder *enc,
                   int64_t timestamp, uint32_t index)
{
    struct log_mgmt_filter filter;
    struct log_walk_ctxt ctxt;
    CborEncoder entries;
    CborError err;
    int rc;

    err = 0;
    err |= cbor_encode_text_stringz(enc, "entries");
    err |= cbor_encoder_create_array(enc, &entries, CborIndefiniteLength);

    filter = (struct log_mgmt_filter) {
        .min_timestamp = timestamp,
        .min_index = index,
    };
    ctxt = (struct log_walk_ctxt) {
        .enc = &entries,
        .rsp_len = cbor_encode_bytes_written(enc),
    };

    rc = log_mgmt_impl_foreach_entry(log->name, &filter,
                                     log_mgmt_cb_encode, &ctxt);
    if (rc != 0 && rc != MGMT_ERR_EMSGSIZE) {
        cbor_encoder_close_container(enc, &entries);
        return rc;
    }

    err |= cbor_encoder_close_container(enc, &entries);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

static int
log_encode(const struct log_mgmt_log *log, CborEncoder *ctxt,
           int64_t timestamp, uint32_t index)
{
    CborEncoder logs;
    CborError err;
    int rc;

    err = 0;
    err |= cbor_encoder_create_map(ctxt, &logs, CborIndefiniteLength);
    err |= cbor_encode_text_stringz(&logs, "name");
    err |= cbor_encode_text_stringz(&logs, log->name);
    err |= cbor_encode_text_stringz(&logs, "type");
    err |= cbor_encode_uint(&logs, log->type);

    rc = log_encode_entries(log, &logs, timestamp, index);
    if (rc != 0) {
        cbor_encoder_close_container(ctxt, &logs);
        return rc;
    }

    err |= cbor_encoder_close_container(ctxt, &logs);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: log show
 */
static int
log_mgmt_show(struct mgmt_ctxt *ctxt)
{
    char name[LOG_MGMT_NAME_LEN];
    struct log_mgmt_log log;
    CborEncoder logs;
    CborError err;
    uint64_t index;
    int64_t timestamp;
    uint32_t next_idx;
    int name_len;
    int log_idx;
    int rc;

    const struct cbor_attr_t attr[] = {
        {
            .attribute = "log_name",
            .type = CborAttrTextStringType,
            .addr.string = name,
            .len = sizeof(name),
        },
        {
            .attribute = "ts",
            .type = CborAttrIntegerType,
            .addr.integer = &timestamp,
        },
        {
            .attribute = "index",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &index,
        },
        {
            .attribute = NULL,
        },
    };

    name[0] = '\0';
    rc = cbor_read_object(&ctxt->it, attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }
    name_len = strlen(name);

    /* Determine the index that the next log entry would use. */
    rc = log_mgmt_impl_get_next_idx(&next_idx);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    err = 0;
    err |= cbor_encode_text_stringz(&ctxt->encoder, "next_index");
    err |= cbor_encode_uint(&ctxt->encoder, next_idx);

    err |= cbor_encode_text_stringz(&ctxt->encoder, "logs");
    err |= cbor_encoder_create_array(&ctxt->encoder, &logs,
                                     CborIndefiniteLength);

    /* Iterate list of logs, encoding each that matches the client request. */
    for (log_idx = 0; ; log_idx++) {
        rc = log_mgmt_impl_get_log(log_idx, &log);
        if (rc == MGMT_ERR_ENOENT) {
            /* Log list fully iterated. */
            if (name_len != 0) {
                /* Client specified log name, but the log wasn't found. */
                cbor_encoder_close_container(&ctxt->encoder, &logs);
                return MGMT_ERR_ENOENT;
            } else {
                break;
            }
        } else if (rc != 0) {
            goto err;
        }

        /* Stream logs cannot be read. */
        if (log.type != LOG_MGMT_TYPE_STREAM) {
            if (name_len == 0 || strcmp(name, log.name) == 0) {
                rc = log_encode(&log, &logs, timestamp, index);

#if LOG_MGMT_READ_WATERMARK_UPDATE
                if (rc == 0 || rc == OS_ENOMEM) {
                    log_mgmt_impl_set_watermark(&log, index);
                }
#endif
                if (rc) {
                    goto err;
                }

#if LOG_MGMT_READ_WATERMARK_UPDATE
                log_mgmt_impl_set_watermark(&log, index);
#endif
                /* If the client specified this log, he isn't interested in the
                 * remaining ones.
                 */
                if (name_len > 0) {
                    break;
                }
            }
        }
    }

err:
    err |= cbor_encoder_close_container(&ctxt->encoder, &logs);
    err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
    err |= cbor_encode_int(&ctxt->encoder, rc);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: log module_list
 */
static int
log_mgmt_module_list(struct mgmt_ctxt *ctxt)
{
    const char *module_name;
    CborEncoder modules;
    CborError err;
    int module;
    int rc;

    err = 0;
    err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
    err |= cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);
    err |= cbor_encode_text_stringz(&ctxt->encoder, "module_map");
    err |= cbor_encoder_create_map(&ctxt->encoder, &modules,
                                   CborIndefiniteLength);

    for (module = 0; ; module++) {
        rc = log_mgmt_impl_get_module(module, &module_name);
        if (rc == MGMT_ERR_ENOENT) {
            break;
        }
        if (rc != 0) {
            cbor_encoder_close_container(&ctxt->encoder, &modules);
            return rc;
        }

        if (module_name != NULL) {
            err |= cbor_encode_text_stringz(&modules, module_name);
            err |= cbor_encode_uint(&modules, module);
        }
    }

    err |= cbor_encoder_close_container(&ctxt->encoder, &modules);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: log list
 */
static int
log_mgmt_logs_list(struct mgmt_ctxt *ctxt)
{
    struct log_mgmt_log log;
    CborEncoder log_list;
    CborError err;
    int log_idx;
    int rc;

    err = 0;
    err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
    err |= cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);
    err |= cbor_encode_text_stringz(&ctxt->encoder, "log_list");
    err |= cbor_encoder_create_array(&ctxt->encoder, &log_list,
                                     CborIndefiniteLength);

    for (log_idx = 0; ; log_idx++) {
        rc = log_mgmt_impl_get_log(log_idx, &log);
        if (rc == MGMT_ERR_ENOENT) {
            break;
        }
        if (rc != 0) {
            cbor_encoder_close_container(&ctxt->encoder, &log_list);
            return rc;
        }

        if (log.type != LOG_MGMT_TYPE_STREAM) {
            err |= cbor_encode_text_stringz(&log_list, log.name);
        }
    }

    err |= cbor_encoder_close_container(&ctxt->encoder, &log_list);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: log level_list
 */
static int
log_mgmt_level_list(struct mgmt_ctxt *ctxt)
{
    const char *level_name;
    CborEncoder level_map;
    CborError err;
    int level;
    int rc;

    err = 0;
    err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
    err |= cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);
    err |= cbor_encode_text_stringz(&ctxt->encoder, "level_map");
    err |= cbor_encoder_create_map(&ctxt->encoder, &level_map,
                                   CborIndefiniteLength);

    for (level = 0; ; level++) {
        rc = log_mgmt_impl_get_level(level, &level_name);
        if (rc == MGMT_ERR_ENOENT) {
            break;
        }
        if (rc != 0) {
            cbor_encoder_close_container(&ctxt->encoder, &level_map);
            return rc;
        }

        if (level_name != NULL) {
            err |= cbor_encode_text_stringz(&level_map, level_name);
            err |= cbor_encode_uint(&level_map, level);
        }
    }

    err |= cbor_encoder_close_container(&ctxt->encoder, &level_map);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: log clear
 */
static int
log_mgmt_clear(struct mgmt_ctxt *ctxt)
{
    struct log_mgmt_log log;
    char name[LOG_MGMT_NAME_LEN] = {0};
    int name_len;
    int log_idx;
    int rc;

    const struct cbor_attr_t attr[] = {
        {
            .attribute = "log_name",
            .type = CborAttrTextStringType,
            .addr.string = name,
            .len = sizeof(name)
        },
        {
            .attribute = NULL
        },
    };

    name[0] = '\0';
    rc = cbor_read_object(&ctxt->it, attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }
    name_len = strlen(name);

    for (log_idx = 0; ; log_idx++) {
        rc = log_mgmt_impl_get_log(log_idx, &log);
        if (rc == MGMT_ERR_ENOENT) {
            return 0;
        }
        if (rc != 0) {
            return rc;
        }

        if (log.type != LOG_MGMT_TYPE_STREAM) {
            if (name_len == 0 || strcmp(log.name, name) == 0) {
                rc = log_mgmt_impl_clear(log.name);
                if (rc != 0) {
                    return rc;
                }

                if (name_len != 0) {
                    return 0;
                }
            }
        }
    }

    if (name_len != 0) {
        return MGMT_ERR_ENOENT;
    }

    return 0;
}

void
log_mgmt_register_group(void)
{
    mgmt_register_group(&log_mgmt_group);
}
