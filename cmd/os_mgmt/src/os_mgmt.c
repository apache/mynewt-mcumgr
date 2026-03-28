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
#include "mgmt/mgmt.h"
#include "os_mgmt/os_mgmt.h"
#include "os_mgmt/os_mgmt_impl.h"
#include "os_mgmt/os_mgmt_config.h"
#if MYNEWT_VAL(OS_MGMT_DATETIME)
#include "datetime/datetime.h"
#endif

#if OS_MGMT_ECHO
static mgmt_handler_fn os_mgmt_echo;
#endif

static mgmt_handler_fn os_mgmt_reset;

#if OS_MGMT_TASKSTAT
static mgmt_handler_fn os_mgmt_taskstat_read;
#endif

#if OS_MGMT_DATETIME
static mgmt_handler_fn os_mgmt_datetime_read;
static mgmt_handler_fn os_mgmt_datetime_write;
#endif

static const struct mgmt_handler os_mgmt_group_handlers[] = {
#if OS_MGMT_ECHO
    [OS_MGMT_ID_ECHO] = {
        os_mgmt_echo, os_mgmt_echo
    },
#endif
#if OS_MGMT_TASKSTAT
    [OS_MGMT_ID_TASKSTAT] = {
        os_mgmt_taskstat_read, NULL
    },
#endif
#if OS_MGMT_DATETIME
    [OS_MGMT_ID_DATETIME_STR] = {
        os_mgmt_datetime_read, os_mgmt_datetime_write
    },
#endif
    [OS_MGMT_ID_RESET] = {
        NULL, os_mgmt_reset
    },
};

#define OS_MGMT_GROUP_SZ    \
    (sizeof os_mgmt_group_handlers / sizeof os_mgmt_group_handlers[0])

static struct mgmt_group os_mgmt_group = {
    .mg_handlers = os_mgmt_group_handlers,
    .mg_handlers_count = OS_MGMT_GROUP_SZ,
    .mg_group_id = MGMT_GROUP_ID_OS,
};

/**
 * Command handler: os echo
 */
#if OS_MGMT_ECHO
static int
os_mgmt_echo(struct mgmt_ctxt *ctxt)
{
    char echo_buf[128];
    int err;

    const struct cbor_attr_t attrs[2] = {
        [0] = {
            .attribute = "d",
            .type = CborAttrTextStringType,
            .addr.string = echo_buf,
            .nodefault = 1,
            .len = sizeof echo_buf,
        },
        [1] = {
            .attribute = NULL
        }
    };

    echo_buf[0] = '\0';

    err = cbor_read_object(&ctxt->decoder, attrs);
    if (err != 0) {
        return MGMT_ERR_EINVAL;
    }

    err  = mgmt_cbor_encode_text_z(&ctxt->encoder, "r");
    err |= mgmt_cbor_encode_text(&ctxt->encoder, echo_buf, strlen(echo_buf));

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}
#endif

#if OS_MGMT_TASKSTAT
/**
 * Encodes a single taskstat entry.
 */
static int
os_mgmt_taskstat_encode_one(mgmt_cbor_encoder_t *encoder,
                            const struct os_mgmt_task_info *task_info)
{
    int err;

    err = 0;
    err |= mgmt_cbor_encode_text_z(encoder, task_info->oti_name);
    err |= mgmt_cbor_map_begin(encoder);
    err |= mgmt_cbor_encode_text_z(encoder, "prio");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_prio);
    err |= mgmt_cbor_encode_text_z(encoder, "tid");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_taskid);
    err |= mgmt_cbor_encode_text_z(encoder, "state");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_state);
    err |= mgmt_cbor_encode_text_z(encoder, "stkuse");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_stkusage);
    err |= mgmt_cbor_encode_text_z(encoder, "stksiz");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_stksize);
    err |= mgmt_cbor_encode_text_z(encoder, "cswcnt");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_cswcnt);
    err |= mgmt_cbor_encode_text_z(encoder, "runtime");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_runtime);
    err |= mgmt_cbor_encode_text_z(encoder, "last_checkin");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_last_checkin);
    err |= mgmt_cbor_encode_text_z(encoder, "next_checkin");
    err |= mgmt_cbor_encode_uint(encoder, task_info->oti_next_checkin);
    err |= mgmt_cbor_map_end(encoder);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: os taskstat
 */
static int
os_mgmt_taskstat_read(struct mgmt_ctxt *ctxt)
{
    struct os_mgmt_task_info task_info;
    int err;
    int task_idx;
    int rc;

    err = 0;
    err |= mgmt_cbor_encode_text_z(&ctxt->encoder, "tasks");
    err |= mgmt_cbor_map_begin(&ctxt->encoder);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    /* Iterate the list of tasks, encoding each. */
    for (task_idx = 0; ; task_idx++) {
        rc = os_mgmt_impl_task_info(task_idx, &task_info);
        if (rc == MGMT_ERR_ENOENT) {
            /* No more tasks to encode. */
            break;
        } else if (rc != 0) {
            mgmt_cbor_map_end(&ctxt->encoder);
            return rc;
        }

        rc = os_mgmt_taskstat_encode_one(&ctxt->encoder, &task_info);
        if (rc != 0) {
            mgmt_cbor_map_end(&ctxt->encoder);
            return rc;
        }
    }

    err = mgmt_cbor_map_end(&ctxt->encoder);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}
#endif

/**
 * Command handler: os datetime
 */
static int
os_mgmt_datetime_read(struct mgmt_ctxt *ctxt)
{
    char buf[DATETIME_BUFSIZE];
    int err;
    int rc;

    err = mgmt_cbor_encode_text_z(&ctxt->encoder, "datetime");
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    rc = os_mgmt_impl_datetime_info(buf, sizeof(buf));
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    err = mgmt_cbor_encode_text_z(&ctxt->encoder, buf);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return MGMT_ERR_EOK;
}

/**
 * Command handler: os datetime
 */
static int
os_mgmt_datetime_write(struct mgmt_ctxt *ctxt)
{
    char datetime_buf[DATETIME_BUFSIZE];
    int err;

    const struct cbor_attr_t attrs[2] = {
        [0] = {
               .attribute = "datetime",
               .type = CborAttrTextStringType,
               .addr.string = datetime_buf,
               .nodefault = 1,
               .len = sizeof datetime_buf,
               },
        [1] = { .attribute = NULL }
    };

    err = cbor_read_object(&ctxt->decoder, attrs);
    if (err != 0) {
        return MGMT_ERR_EINVAL;
    }

    err = os_mgmt_impl_datetime_set(datetime_buf);
    if (err != 0) {
        return MGMT_ERR_ECORRUPT;
    }

    return MGMT_ERR_EOK;
}

/**
 * Command handler: os reset
 */
static int
os_mgmt_reset(struct mgmt_ctxt *ctxt)
{
    return os_mgmt_impl_reset(OS_MGMT_RESET_MS);
}

void
os_mgmt_register_group(void)
{
    mgmt_register_group(&os_mgmt_group);
}

void
os_mgmt_module_init(void)
{
    os_mgmt_register_group();
}
