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

#ifndef H_LOG_MGMT_IMPL_
#define H_LOG_MGMT_IMPL_

#ifdef __cplusplus
extern "C" {
#endif

struct log_mgmt_filter;
struct log_mgmt_entry;
struct log_mgmt_log;

typedef int log_mgmt_foreach_entry_fn(const struct log_mgmt_entry *entry,
                                      void *arg);

int log_mgmt_impl_get_log(int idx, struct log_mgmt_log *out_log);
int log_mgmt_impl_get_module(int idx, const char **out_module_name);
int log_mgmt_impl_get_level(int idx, const char **out_level_name);
int log_mgmt_impl_get_next_idx(uint32_t *out_idx);
int log_mgmt_impl_foreach_entry(const char *log_name,
                                const struct log_mgmt_filter *filter,
                                log_mgmt_foreach_entry_fn *cb,
                                void *arg);
int log_mgmt_impl_clear(const char *log_name);

#ifdef __cplusplus
}
#endif

#endif
