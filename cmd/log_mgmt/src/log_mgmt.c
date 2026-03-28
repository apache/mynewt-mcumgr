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
#include "log_mgmt/log_mgmt.h"
#include "log_mgmt/log_mgmt_impl.h"
#include "log_mgmt/log_mgmt_config.h"
#include "log/log.h"

/** Context used during walks. */
struct log_walk_ctxt {
    /* last encoded index */
    uint32_t last_enc_index;
    /* The number of bytes encoded to the response so far. */
    size_t rsp_len;
    /* The encoder to use to write the current log entry. */
    mgmt_cbor_encoder_t *enc;
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
log_mgmt_encode_entry(mgmt_cbor_encoder_t *enc,
                      const struct log_mgmt_entry *entry,
                      size_t *out_len)
{
    size_t len_before;
    int err;

    len_before = mgmt_cbor_encoder_bytes_written(enc);

    /* Only encode complete entries (offset == 0 means start of entry). */
    if (entry->offset != 0) {
        if (out_len) {
            *out_len = 0;
        }
        return LOG_MGMT_ERR_EOK;
    }

    err = mgmt_cbor_map_begin(enc);

    switch (entry->type) {
    case LOG_MGMT_ETYPE_CBOR:
        err |= mgmt_cbor_encode_text_z(enc, "type");
        err |= mgmt_cbor_encode_text_z(enc, "cbor");
        break;
    case LOG_MGMT_ETYPE_BINARY:
        err |= mgmt_cbor_encode_text_z(enc, "type");
        err |= mgmt_cbor_encode_text_z(enc, "bin");
        break;
    case LOG_MGMT_ETYPE_STRING:
        err |= mgmt_cbor_encode_text_z(enc, "type");
        err |= mgmt_cbor_encode_text_z(enc, "str");
        break;
    default:
        mgmt_cbor_map_end(enc);
        return LOG_MGMT_ERR_ECORRUPT;
    }

    err |= mgmt_cbor_encode_text_z(enc, "ts");
    err |= mgmt_cbor_encode_int(enc, entry->ts);
    err |= mgmt_cbor_encode_text_z(enc, "level");
    err |= mgmt_cbor_encode_uint(enc, entry->level);
    err |= mgmt_cbor_encode_text_z(enc, "index");
    err |= mgmt_cbor_encode_uint(enc, entry->index);
    err |= mgmt_cbor_encode_text_z(enc, "module");
    err |= mgmt_cbor_encode_uint(enc, entry->module);

    if (entry->flags & LOG_MGMT_FLAGS_IMG_HASH) {
        err |= mgmt_cbor_encode_text_z(enc, "imghash");
        err |= mgmt_cbor_encode_bytes(enc, entry->imghash,
                                      LOG_MGMT_IMG_HASHLEN);
    }

    err |= mgmt_cbor_encode_text_z(enc, "msg");
    err |= mgmt_cbor_encode_bytes(enc, entry->data, entry->chunklen);

    err |= mgmt_cbor_map_end(enc);

    if (out_len) {
        *out_len = mgmt_cbor_encoder_bytes_written(enc) - len_before;
    }

    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
    }

    return LOG_MGMT_ERR_EOK;
}

static int
log_mgmt_cb_encode(struct log_mgmt_entry *entry, void *arg)
{
    struct log_walk_ctxt *ctxt;
    size_t entry_len;
    int rc;

    ctxt = arg;

    if (entry->offset == 0) {
        /*
         * Estimate entry size by encoding into a scratch buffer.
         * We record the encoder position before and after a dry-run on the
         * real encoder.  If the entry is too large we abort and report the
         * error without having written a partial entry (map_begin/map_end
         * are balanced, so the encoder state is still valid after an error).
         */
        size_t before = mgmt_cbor_encoder_bytes_written(ctxt->enc);
        rc = log_mgmt_encode_entry(ctxt->enc, entry, &entry_len);
        if (rc != 0) {
            return rc;
        }

        /* `+ 1` to account for the CBOR array terminator. */
        if (before + entry_len + 1 > LOG_MGMT_MAX_RSP_LEN) {
            if (ctxt->counter == 0) {
                entry->type = LOG_ETYPE_STRING;
                snprintf((char *)entry->data, LOG_MGMT_MAX_RSP_LEN,
                         "error: entry too large (%zu bytes)", entry_len);
            }
            return -1 * LOG_MGMT_ERR_EUNKNOWN;
        }
        ctxt->rsp_len += entry_len;
    } else {
        /* Skip non-first chunks; full entry encoded on offset==0. */
        return LOG_MGMT_ERR_EOK;
    }

    ctxt->counter++;
    ctxt->last_enc_index = entry->index;

    return 0;
}

static int
log_encode_entries(const struct log_mgmt_log *log, mgmt_cbor_encoder_t *enc,
                   int64_t timestamp, uint32_t index)
{
    struct log_mgmt_filter filter;
    struct log_walk_ctxt ctxt;
    int err;
    int rc;

    err = 0;
    err |= mgmt_cbor_encode_text_z(enc, "entries");
    err |= mgmt_cbor_array_begin(enc);
    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
    }

    filter = (struct log_mgmt_filter) {
        .min_timestamp = timestamp,
        .min_index = index,
    };
    ctxt = (struct log_walk_ctxt) {
        .enc = enc,
        .rsp_len = mgmt_cbor_encoder_bytes_written(enc),
    };

    rc = log_mgmt_impl_foreach_entry(log->name, &filter,
                                     log_mgmt_cb_encode, &ctxt);
    if (rc < 0) {
        rc = -1 * rc;
    }

    err |= mgmt_cbor_array_end(enc);
    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
    }

#if LOG_MGMT_READ_WATERMARK_UPDATE
    if (!rc || rc == LOG_MGMT_ERR_EUNKNOWN) {
        log_mgmt_impl_set_watermark(log, ctxt.last_enc_index);
    }
#endif
    return rc;
}

static int
log_encode(const struct log_mgmt_log *log, mgmt_cbor_encoder_t *enc,
           int64_t timestamp, uint32_t index)
{
    int err;
    int rc;

    err = mgmt_cbor_map_begin(enc);
    err |= mgmt_cbor_encode_text_z(enc, "name");
    err |= mgmt_cbor_encode_text_z(enc, log->name);
    err |= mgmt_cbor_encode_text_z(enc, "type");
    err |= mgmt_cbor_encode_uint(enc, log->type);

    rc = log_encode_entries(log, enc, timestamp, index);
    if (rc != 0) {
        mgmt_cbor_map_end(enc);
        return rc;
    }

    err |= mgmt_cbor_map_end(enc);
    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
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
    int err;
    uint64_t index;
    uint32_t next_idx;
    int64_t timestamp;
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
    rc = cbor_read_object(&ctxt->decoder, attr);
    if (rc != 0) {
        return LOG_MGMT_ERR_EINVAL;
    }
    name_len = strlen(name);

    err = 0;
#if LOG_MGMT_GLOBAL_IDX
    rc = log_mgmt_impl_get_next_idx(&next_idx);
    if (rc != 0) {
        return LOG_MGMT_ERR_EUNKNOWN;
    }
#else
    next_idx = 0;
#endif

    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "next_index");
    err |= mgmt_cbor_encode_uint(&ctxt->encoder, next_idx);

    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "logs");
    err |= mgmt_cbor_array_begin(&ctxt->encoder);

    for (log_idx = 0; ; log_idx++) {
        rc = log_mgmt_impl_get_log(log_idx, &log);
        if (rc == LOG_MGMT_ERR_ENOENT) {
            if (name_len != 0) {
                mgmt_cbor_array_end(&ctxt->encoder);
                return LOG_MGMT_ERR_ENOENT;
            } else {
                break;
            }
        } else if (rc != 0) {
            goto err;
        }

        if (log.type != LOG_MGMT_TYPE_STREAM) {
            if (name_len == 0 || strcmp(name, log.name) == 0) {
                rc = log_encode(&log, &ctxt->encoder, timestamp, index);
                if (rc) {
                    goto err;
                }

                if (name_len > 0) {
                    break;
                }
            }
        }
    }

err:
    err |= mgmt_cbor_array_end(&ctxt->encoder);
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "rc");
    err |= mgmt_cbor_encode_int(&ctxt->encoder, rc);

    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
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
    int err;
    int module;
    int rc;

    err = 0;
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "rc");
    err |= mgmt_cbor_encode_int(&ctxt->encoder, LOG_MGMT_ERR_EOK);
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "module_map");
    err |= mgmt_cbor_map_begin(&ctxt->encoder);

    for (module = 0; ; module++) {
        rc = log_mgmt_impl_get_module(module, &module_name);
        if (rc == LOG_MGMT_ERR_ENOENT) {
            break;
        }
        if (rc != 0) {
            mgmt_cbor_map_end(&ctxt->encoder);
            return rc;
        }

        if (module_name != NULL) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, module_name);
            err |= mgmt_cbor_encode_uint(&ctxt->encoder, module);
        }
    }

    err |= mgmt_cbor_map_end(&ctxt->encoder);

    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
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
    int err;
    int log_idx;
    int rc;

    err = 0;
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "rc");
    err |= mgmt_cbor_encode_int(&ctxt->encoder, LOG_MGMT_ERR_EOK);
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "log_list");
    err |= mgmt_cbor_array_begin(&ctxt->encoder);

    for (log_idx = 0; ; log_idx++) {
        rc = log_mgmt_impl_get_log(log_idx, &log);
        if (rc == LOG_MGMT_ERR_ENOENT) {
            break;
        }
        if (rc != 0) {
            mgmt_cbor_array_end(&ctxt->encoder);
            return rc;
        }

        if (log.type != LOG_MGMT_TYPE_STREAM) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, log.name);
        }
    }

    err |= mgmt_cbor_array_end(&ctxt->encoder);

    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
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
    int err;
    int level;
    int rc;

    err = 0;
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "rc");
    err |= mgmt_cbor_encode_int(&ctxt->encoder, LOG_MGMT_ERR_EOK);
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "level_map");
    err |= mgmt_cbor_map_begin(&ctxt->encoder);

    for (level = 0; ; level++) {
        rc = log_mgmt_impl_get_level(level, &level_name);
        if (rc == LOG_MGMT_ERR_ENOENT) {
            break;
        }
        if (rc != 0) {
            mgmt_cbor_map_end(&ctxt->encoder);
            return rc;
        }

        if (level_name != NULL) {
            err |= mgmt_cbor_encode_text_z(&ctxt->encoder, level_name);
            err |= mgmt_cbor_encode_uint(&ctxt->encoder, level);
        }
    }

    err |= mgmt_cbor_map_end(&ctxt->encoder);

    if (err != 0) {
        return LOG_MGMT_ERR_ENOMEM;
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
    rc = cbor_read_object(&ctxt->decoder, attr);
    if (rc != 0) {
        return LOG_MGMT_ERR_EINVAL;
    }
    name_len = strlen(name);

    for (log_idx = 0; ; log_idx++) {
        rc = log_mgmt_impl_get_log(log_idx, &log);
        if (rc == LOG_MGMT_ERR_ENOENT) {
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
        return LOG_MGMT_ERR_ENOENT;
    }

    return 0;
}

void
log_mgmt_register_group(void)
{
    mgmt_register_group(&log_mgmt_group);
}
