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

#ifndef H_IMG_MGMT_
#define H_IMG_MGMT_

#include <inttypes.h>
struct image_version;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Image state flags
 */
#define IMG_MGMT_STATE_F_PENDING    0x01
#define IMG_MGMT_STATE_F_CONFIRMED  0x02
#define IMG_MGMT_STATE_F_ACTIVE     0x04
#define IMG_MGMT_STATE_F_PERMANENT  0x08

#define IMG_MGMT_VER_MAX_STR_LEN    25  /* 255.255.65535.4294967295\0 */

/*
 * Swap Types for image management state machine
 */
#define IMG_MGMT_SWAP_TYPE_NONE     0
#define IMG_MGMT_SWAP_TYPE_TEST     1
#define IMG_MGMT_SWAP_TYPE_PERM     2
#define IMG_MGMT_SWAP_TYPE_REVERT   3

/**
 * Command IDs for image management group.
 */
#define IMG_MGMT_ID_STATE           0
#define IMG_MGMT_ID_UPLOAD          1
#define IMG_MGMT_ID_FILE            2
#define IMG_MGMT_ID_CORELIST        3
#define IMG_MGMT_ID_CORELOAD        4
#define IMG_MGMT_ID_ERASE           5

/*
 * IMG_MGMT_ID_UPLOAD statuses.
 */
#define IMG_MGMT_ID_UPLOAD_STATUS_START         0
#define IMG_MGMT_ID_UPLOAD_STATUS_ONGOING       1
#define IMG_MGMT_ID_UPLOAD_STATUS_COMPLETE      2

/**
 * @brief Registers the image management command handler group.
 */ 
void img_mgmt_register_group(void);
/**
 * @brief Read info of an image give the slot number
 */
int img_mgmt_read_info(int image_slot, struct image_version *ver,
                       uint8_t *hash, uint32_t *flags);
/**
 * @brief Get the current running image version
 */
int
img_mgmt_my_version(struct image_version *ver);

#ifdef __cplusplus
}
#endif

#endif /* H_IMG_MGMT_ */
