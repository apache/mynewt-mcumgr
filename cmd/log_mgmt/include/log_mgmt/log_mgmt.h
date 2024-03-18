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

#ifndef H_LOG_MGMT_
#define H_LOG_MGMT_

#ifdef __cplusplus
extern "C" {
#endif

#include "log_mgmt_config.h"

/**
 * LOG MGMT specific error codes, 0 -> 6, 8 are same as mcumgr,
 * 7 and 9 are different for backwards compatibility with newtmgr.
 */
#define LOG_MGMT_ERR_EOK        (0)
#define LOG_MGMT_ERR_EUNKNOWN   (1)
#define LOG_MGMT_ERR_ENOMEM     (2)
#define LOG_MGMT_ERR_EINVAL     (3)
#define LOG_MGMT_ERR_ETIMEOUT   (4)
#define LOG_MGMT_ERR_ENOENT     (5)
#define LOG_MGMT_ERR_EBADSTATE  (6)     /* Current state disallows command. */
#define LOG_MGMT_ERR_ECORRUPT   (7)
#define LOG_MGMT_ERR_ENOTSUP    (8)
#define LOG_MGMT_ERR_EMSGSIZE   (9)
#define LOG_MGMT_ERR_EPERUSER   (256)

/**
 * Command IDs for log management group.
 */
#define LOG_MGMT_ID_SHOW           0
#define LOG_MGMT_ID_CLEAR          1
#define LOG_MGMT_ID_APPEND         2
#define LOG_MGMT_ID_MODULE_LIST    3
#define LOG_MGMT_ID_LEVEL_LIST     4
#define LOG_MGMT_ID_LOGS_LIST      5
#define LOG_MGMT_ID_NUM_ENTRIES    6

/** @brief Log output is streamed without retention (e.g., console). */
#define LOG_MGMT_TYPE_STREAM     0

/** @brief Log entries are stored in RAM. */
#define LOG_MGMT_TYPE_MEMORY     1

/** @brief Log entries are persisted across reboots. */
#define LOG_MGMT_TYPE_STORAGE    2

/* @brief Flags used to indicate type of data in reserved payload. */
#define LOG_MGMT_FLAGS_IMG_HASH     (1 << 0)
#define LOG_MGMT_FLAGS_NUM_ENTRIES  (1 << 1)

/* @brief Log entry types. */
#define LOG_MGMT_ETYPE_STRING         0
#define LOG_MGMT_ETYPE_CBOR           1
#define LOG_MGMT_ETYPE_BINARY         2


/** @brief Generic descriptor for an OS-specific log. */
struct log_mgmt_log {
    const char *name;
    int type;
#if !LOG_MGMT_GLOBAL_IDX
    uint32_t index;
#endif
};

/** @brief Generic descriptor for an OS-specific log entry. */
struct log_mgmt_entry {
    int64_t ts;
    uint32_t index;
    const void *data;
    size_t len;
    uint8_t module;
    uint8_t level;
    uint8_t type:4;
    uint8_t flags:4;
    const uint8_t *imghash;
    uint32_t num_entries;
    size_t offset;
    size_t chunklen;
    void *ctxt;
};

/** @brief Indicates which log entries to operate on. */
struct log_mgmt_filter {
    /* If   min_timestamp == -1: Only access last log entry;
     * Elif min_timestamp == 0:  Don't filter by timestamp;
     * Else:                     Only access entries whose ts >= min_timestamp.
     */
    int64_t min_timestamp;

    /* Only access entries whose index >= min_index. */
    uint32_t min_index;
};

/**
 * @brief Registers the log management command handler group.
 */
void log_mgmt_register_group(void);

#ifdef __cplusplus
}
#endif

#endif
