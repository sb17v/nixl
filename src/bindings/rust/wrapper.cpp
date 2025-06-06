/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "wrapper.h"

#include <nixl.h>
#include <nixl_types.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <map>
#include <string>
#include <vector>

extern "C" {

// Internal struct definitions to match our opaque types
struct nixl_capi_agent_s {
  nixlAgent* inner;
};

struct nixl_capi_string_list_s {
  std::vector<std::string> strings;
};

struct nixl_capi_params_s {
  nixl_b_params_t params;
};

struct nixl_capi_mem_list_s {
  nixl_mem_list_t mems;
};

struct nixl_capi_backend_s {
  nixlBackendH* backend;
};

struct nixl_capi_opt_args_s {
  nixl_opt_args_t args;
};

struct nixl_capi_param_iter_s {
  nixl_b_params_t::iterator current;
  nixl_b_params_t::iterator end;
  std::string current_key;    // Keep string alive while iterator exists
  std::string current_value;  // Keep string alive while iterator exists
};

// Internal structs for descriptor lists
struct nixl_capi_xfer_dlist_s {
  nixl_xfer_dlist_t* dlist;
};

struct nixl_capi_reg_dlist_s {
  nixl_reg_dlist_t* dlist;
};

// Internal struct for transfer request handle
struct nixl_capi_xfer_req_s {
  nixlXferReqH* req;
};

struct nixl_capi_notif_map_s {
  nixl_notifs_t notif_map;
};

nixl_capi_status_t
nixl_capi_create_agent(const char* name, nixl_capi_agent_t* agent)
{
  if (!name || !agent) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixlAgentConfig nixl_config(true);  // Use progress thread
    std::string agent_name = name;
    auto inner = new nixlAgent(agent_name, nixl_config);

    auto agent_handle = new nixl_capi_agent_s;
    agent_handle->inner = inner;
    *agent = agent_handle;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_agent(nixl_capi_agent_t agent)
{
  if (!agent) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete agent->inner;
    delete agent;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_get_local_md(nixl_capi_agent_t agent, void** data, size_t* len)
{
  if (!agent || !data || !len) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixl_blob_t blob;
    nixl_status_t ret = agent->inner->getLocalMD(blob);
    if (ret != NIXL_SUCCESS) {
      return NIXL_CAPI_ERROR_BACKEND;
    }

    // Allocate memory for the blob data
    void* blob_data = malloc(blob.size());
    if (!blob_data) {
      return NIXL_CAPI_ERROR_BACKEND;
    }

    // Copy the data
    memcpy(blob_data, blob.data(), blob.size());
    *data = blob_data;
    *len = blob.size();

    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_load_remote_md(nixl_capi_agent_t agent, const void* data, size_t len, char** agent_name)
{
  if (!agent || !data || !len || !agent_name) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    // Create blob from input data - use the constructor that takes void*
    nixl_blob_t blob;
    blob.assign((const char*)data, len);
    std::string name;

    // Load the metadata
    nixl_status_t ret = agent->inner->loadRemoteMD(blob, name);
    if (ret != NIXL_SUCCESS) {
      return NIXL_CAPI_ERROR_BACKEND;
    }

    // Allocate and copy the agent name
    char* name_str = strdup(name.c_str());
    if (!name_str) {
      return NIXL_CAPI_ERROR_BACKEND;
    }
    *agent_name = name_str;

    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_invalidate_remote_md(nixl_capi_agent_t agent, const char* remote_agent)
{
  if (!agent || !remote_agent) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixl_status_t ret = agent->inner->invalidateRemoteMD(std::string(remote_agent));
    return ret == NIXL_SUCCESS ? NIXL_CAPI_SUCCESS : NIXL_CAPI_ERROR_BACKEND;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_get_available_plugins(nixl_capi_agent_t agent, nixl_capi_string_list_t* plugins)
{
  if (!agent || !plugins) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    std::vector<nixl_backend_t> backend_plugins;
    nixl_status_t ret = agent->inner->getAvailPlugins(backend_plugins);

    if (ret != NIXL_SUCCESS) {
      return NIXL_CAPI_ERROR_BACKEND;
    }

    auto list = new nixl_capi_string_list_s;
    list->strings = std::move(backend_plugins);
    *plugins = list;

    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_string_list(nixl_capi_string_list_t list)
{
  if (!list) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete list;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_string_list_size(nixl_capi_string_list_t list, size_t* size)
{
  if (!list || !size) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *size = list->strings.size();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_string_list_get(nixl_capi_string_list_t list, size_t index, const char** str)
{
  if (!list || !str || index >= list->strings.size()) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *str = list->strings[index].c_str();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_get_plugin_params(
    nixl_capi_agent_t agent, const char* plugin_name, nixl_capi_mem_list_t* mems, nixl_capi_params_t* params)
{
  if (!agent || !plugin_name || !mems || !params) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto mem_list = new nixl_capi_mem_list_s;
    auto param_list = new nixl_capi_params_s;

    nixl_status_t ret = agent->inner->getPluginParams(plugin_name, mem_list->mems, param_list->params);

    if (ret != NIXL_SUCCESS) {
      delete mem_list;
      delete param_list;
      return NIXL_CAPI_ERROR_BACKEND;
    }

    *mems = mem_list;
    *params = param_list;

    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_mem_list(nixl_capi_mem_list_t list)
{
  if (!list) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete list;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_params(nixl_capi_params_t params)
{
  if (!params) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete params;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_create_backend(
    nixl_capi_agent_t agent, const char* plugin_name, nixl_capi_params_t params, nixl_capi_backend_t* backend)
{
  if (!agent || !plugin_name || !params || !backend) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto backend_handle = new nixl_capi_backend_s;
    nixl_status_t ret = agent->inner->createBackend(plugin_name, params->params, backend_handle->backend);

    if (ret != NIXL_SUCCESS) {
      delete backend_handle;
      return NIXL_CAPI_ERROR_BACKEND;
    }

    *backend = backend_handle;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_backend(nixl_capi_backend_t backend)
{
  if (!backend) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete backend;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_create_opt_args(nixl_capi_opt_args_t* args)
{
  if (!args) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto opt_args = new nixl_capi_opt_args_s;
    *args = opt_args;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_opt_args(nixl_capi_opt_args_t args)
{
  if (!args) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete args;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_add_backend(nixl_capi_opt_args_t args, nixl_capi_backend_t backend)
{
  if (!args || !backend) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    args->args.backends.push_back(backend->backend);
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_set_notif_msg(nixl_capi_opt_args_t args, const void* data, size_t len)
{
  if (!args || (!data && len > 0)) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    args->args.notifMsg.assign((const char*)data, len);
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_get_notif_msg(nixl_capi_opt_args_t args, void** data, size_t* len)
{
  if (!args || !data || !len) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    size_t msg_size = args->args.notifMsg.size();
    if (msg_size == 0) {
      *data = nullptr;
      *len = 0;
      return NIXL_CAPI_SUCCESS;
    }

    void* msg_data = malloc(msg_size);
    if (!msg_data) {
      return NIXL_CAPI_ERROR_BACKEND;
    }

    memcpy(msg_data, args->args.notifMsg.data(), msg_size);
    *data = msg_data;
    *len = msg_size;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_set_has_notif(nixl_capi_opt_args_t args, bool has_notif)
{
  if (!args) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    args->args.hasNotif = has_notif;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_get_has_notif(nixl_capi_opt_args_t args, bool* has_notif)
{
  if (!args || !has_notif) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *has_notif = args->args.hasNotif;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_set_skip_desc_merge(nixl_capi_opt_args_t args, bool skip_merge)
{
  if (!args) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    args->args.skipDescMerge = skip_merge;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_opt_args_get_skip_desc_merge(nixl_capi_opt_args_t args, bool* skip_merge)
{
  if (!args || !skip_merge) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *skip_merge = args->args.skipDescMerge;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_params_is_empty(nixl_capi_params_t params, bool* is_empty)
{
  if (!params || !is_empty) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *is_empty = params->params.empty();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_params_create_iterator(nixl_capi_params_t params, nixl_capi_param_iter_t* iter)
{
  if (!params || !iter) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto param_iter = new nixl_capi_param_iter_s;
    param_iter->current = params->params.begin();
    param_iter->end = params->params.end();
    *iter = param_iter;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_params_iterator_next(nixl_capi_param_iter_t iter, const char** key, const char** value, bool* has_next)
{
  if (!iter || !key || !value || !has_next) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    if (iter->current == iter->end) {
      *has_next = false;
      return NIXL_CAPI_SUCCESS;
    }

    // Store the strings in the iterator to keep them alive
    iter->current_key = iter->current->first;
    iter->current_value = iter->current->second;

    *key = iter->current_key.c_str();
    *value = iter->current_value.c_str();

    ++iter->current;
    *has_next = (iter->current != iter->end);

    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_params_destroy_iterator(nixl_capi_param_iter_t iter)
{
  if (!iter) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete iter;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_mem_list_is_empty(nixl_capi_mem_list_t list, bool* is_empty)
{
  if (!list || !is_empty) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *is_empty = list->mems.empty();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_mem_list_size(nixl_capi_mem_list_t list, size_t* size)
{
  if (!list || !size) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *size = list->mems.size();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_mem_list_get(nixl_capi_mem_list_t list, size_t index, nixl_capi_mem_type_t* mem_type)
{
  if (!list || !mem_type || index >= list->mems.size()) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *mem_type = static_cast<nixl_capi_mem_type_t>(list->mems[index]);
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_mem_type_to_string(nixl_capi_mem_type_t mem_type, const char** str)
{
  if (!str) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    static const char* mem_type_strings[] = {
        "DRAM",
        "VRAM",
        "BLOCK",
        "OBJECT",
        "FILE",
        "UNKNOWN"
    };

    if (mem_type < 0 || mem_type >= sizeof(mem_type_strings) / sizeof(mem_type_strings[0])) {
      return NIXL_CAPI_ERROR_INVALID_PARAM;
    }

    *str = mem_type_strings[mem_type];
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_get_backend_params(
    nixl_capi_agent_t agent, nixl_capi_backend_t backend, nixl_capi_mem_list_t* mems, nixl_capi_params_t* params)
{
  if (!agent || !backend || !mems || !params) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto mem_list = new nixl_capi_mem_list_s;
    auto param_list = new nixl_capi_params_s;

    nixl_status_t ret = agent->inner->getBackendParams(backend->backend, mem_list->mems, param_list->params);

    if (ret != NIXL_SUCCESS) {
      delete mem_list;
      delete param_list;
      return NIXL_CAPI_ERROR_BACKEND;
    }

    *mems = mem_list;
    *params = param_list;

    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

// Transfer descriptor list functions
nixl_capi_status_t
nixl_capi_create_xfer_dlist(nixl_capi_mem_type_t mem_type, nixl_capi_xfer_dlist_t* dlist)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto d = new nixl_capi_xfer_dlist_s;
    d->dlist = new nixl_xfer_dlist_t(static_cast<nixl_mem_t>(mem_type));
    *dlist = d;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_xfer_dlist(nixl_capi_xfer_dlist_t dlist)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete dlist->dlist;
    delete dlist;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_xfer_dlist_add_desc(nixl_capi_xfer_dlist_t dlist, uintptr_t addr, size_t len, uint64_t dev_id)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixlBasicDesc desc(addr, len, dev_id);
    dlist->dlist->addDesc(desc);
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_xfer_dlist_len(nixl_capi_xfer_dlist_t dlist, size_t* len)
{
  if (!dlist || !len) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *len = dlist->dlist->descCount();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_xfer_dlist_has_overlaps(nixl_capi_xfer_dlist_t dlist, bool* has_overlaps)
{
  if (!dlist || !has_overlaps) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *has_overlaps = dlist->dlist->hasOverlaps();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_xfer_dlist_clear(nixl_capi_xfer_dlist_t dlist)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    dlist->dlist->clear();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_xfer_dlist_resize(nixl_capi_xfer_dlist_t dlist, size_t new_size)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    dlist->dlist->resize(new_size);
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

// Registration descriptor list functions
nixl_capi_status_t
nixl_capi_create_reg_dlist(nixl_capi_mem_type_t mem_type, nixl_capi_reg_dlist_t* dlist)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto d = new nixl_capi_reg_dlist_s;
    d->dlist = new nixl_reg_dlist_t(static_cast<nixl_mem_t>(mem_type));
    *dlist = d;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_reg_dlist(nixl_capi_reg_dlist_t dlist)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete dlist->dlist;
    delete dlist;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_reg_dlist_add_desc(nixl_capi_reg_dlist_t dlist, uintptr_t addr, size_t len, uint64_t dev_id)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixlBlobDesc desc(addr, len, dev_id);  // Empty metadata
    dlist->dlist->addDesc(desc);
#ifdef NIXL_DEBUG
    printf("** Adding descriptor\n");
    dlist->dlist->print();
    printf("** Added descriptor\n");
#endif
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_reg_dlist_len(nixl_capi_reg_dlist_t dlist, size_t* len)
{
  if (!dlist || !len) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *len = dlist->dlist->descCount();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_reg_dlist_has_overlaps(nixl_capi_reg_dlist_t dlist, bool* has_overlaps)
{
  if (!dlist || !has_overlaps) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *has_overlaps = dlist->dlist->hasOverlaps();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_reg_dlist_clear(nixl_capi_reg_dlist_t dlist)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    dlist->dlist->clear();
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_reg_dlist_resize(nixl_capi_reg_dlist_t dlist, size_t new_size)
{
  if (!dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    dlist->dlist->resize(new_size);
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

// Memory registration functions
nixl_capi_status_t
nixl_capi_register_mem(nixl_capi_agent_t agent, nixl_capi_reg_dlist_t dlist, nixl_capi_opt_args_t opt_args)
{
  if (!agent || !dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
#ifdef NIXL_DEBUG
    printf("** Registering memory\n");
    printf("** Backend Count: %ld\n", opt_args ? opt_args->args.backends.size() : 0);
    printf("** Descriptor list:\n");
    dlist->dlist->print();
    printf("** Registered memory\n");
#endif
    nixl_status_t ret = agent->inner->registerMem(*dlist->dlist, opt_args ? &opt_args->args : nullptr);
    return ret == NIXL_SUCCESS ? NIXL_CAPI_SUCCESS : NIXL_CAPI_ERROR_BACKEND;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_deregister_mem(nixl_capi_agent_t agent, nixl_capi_reg_dlist_t dlist, nixl_capi_opt_args_t opt_args)
{
  if (!agent || !dlist) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
#ifdef NIXL_DEBUG
    printf("** Deregistering memory\n");
    dlist->dlist->print();
    printf("** Deregistered memory\n");
#endif
    nixl_status_t ret = agent->inner->deregisterMem(*dlist->dlist, opt_args ? &opt_args->args : nullptr);
    return ret == NIXL_SUCCESS ? NIXL_CAPI_SUCCESS : NIXL_CAPI_ERROR_BACKEND;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}


nixl_capi_status_t
nixl_capi_create_xfer_req(
    nixl_capi_agent_t agent, nixl_capi_xfer_op_t operation, nixl_capi_xfer_dlist_t local_descs,
    nixl_capi_xfer_dlist_t remote_descs, const char* remote_agent, nixl_capi_xfer_req_t* req_hndl,
    nixl_capi_opt_args_t opt_args)
{
  if (!agent || !local_descs || !remote_descs || !remote_agent || !req_hndl) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto req = new nixl_capi_xfer_req_s;
    nixl_status_t ret = agent->inner->createXferReq(
        static_cast<nixl_xfer_op_t>(operation), *local_descs->dlist, *remote_descs->dlist, std::string(remote_agent),
        req->req, opt_args ? &opt_args->args : nullptr);

    if (ret != NIXL_SUCCESS) {
      delete req;
      return NIXL_CAPI_ERROR_BACKEND;
    }

    *req_hndl = req;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_post_xfer_req(nixl_capi_agent_t agent, nixl_capi_xfer_req_t req_hndl, nixl_capi_opt_args_t opt_args)
{
  if (!agent || !req_hndl) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixl_status_t ret = agent->inner->postXferReq(req_hndl->req, opt_args ? &opt_args->args : nullptr);

    return ret == NIXL_SUCCESS ? NIXL_CAPI_SUCCESS : ret == NIXL_IN_PROG ? NIXL_CAPI_IN_PROG : NIXL_CAPI_ERROR_BACKEND;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_get_xfer_status(nixl_capi_agent_t agent, nixl_capi_xfer_req_t req_hndl)
{
  if (!agent || !req_hndl) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixl_status_t ret = agent->inner->getXferStatus(req_hndl->req);
    return ret == NIXL_SUCCESS ? NIXL_CAPI_SUCCESS : ret == NIXL_IN_PROG ? NIXL_CAPI_IN_PROG : NIXL_CAPI_ERROR_BACKEND;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_xfer_req(nixl_capi_xfer_req_t req)
{
  if (!req) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  if (req->req) {
    return NIXL_CAPI_ERROR_INVALID_STATE;
  }

  try {
    delete req;
    return NIXL_CAPI_SUCCESS;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_release_xfer_req(nixl_capi_agent_t agent, nixl_capi_xfer_req_t req)
{
  if (!agent || !req || !req->req) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixl_status_t ret = agent->inner->releaseXferReq(req->req);
    if (ret == NIXL_SUCCESS) {
      req->req = nullptr;  // Prevent double-free in destroy
    }
    return ret == NIXL_SUCCESS ? NIXL_CAPI_SUCCESS : NIXL_CAPI_ERROR_BACKEND;
  }
  catch (...) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_get_notifs(nixl_capi_agent_t agent, nixl_capi_notif_map_t notif_map, nixl_capi_opt_args_t opt_args)
{
  if (!agent || !notif_map) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    nixl_status_t ret = agent->inner->getNotifs(notif_map->notif_map, opt_args ? &opt_args->args : nullptr);
    if (ret != NIXL_SUCCESS) {
      return NIXL_CAPI_ERROR_BACKEND;
    }
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_create_notif_map(nixl_capi_notif_map_t* notif_map)
{
  if (!notif_map) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto map = new nixl_capi_notif_map_s;
    *notif_map = map;
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_destroy_notif_map(nixl_capi_notif_map_t notif_map)
{
  if (!notif_map) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    delete notif_map;
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_notif_map_size(nixl_capi_notif_map_t map, size_t* size)
{
  if (!map || !size) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    *size = map->notif_map.size();
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_notif_map_get_agent_at(nixl_capi_notif_map_t map, size_t index, const char** agent_name)
{
  if (!map || !agent_name) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto it = map->notif_map.begin();
    std::advance(it, index);
    if (it == map->notif_map.end()) {
      return NIXL_CAPI_ERROR_INVALID_PARAM;
    }
    *agent_name = it->first.c_str();
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_notif_map_get_notifs_size(nixl_capi_notif_map_t map, const char* agent_name, size_t* size)
{
  if (!map || !agent_name || !size) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto it = map->notif_map.find(agent_name);
    if (it == map->notif_map.end()) {
      return NIXL_CAPI_ERROR_INVALID_PARAM;
    }
    *size = it->second.size();
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

nixl_capi_status_t
nixl_capi_notif_map_get_notif(
    nixl_capi_notif_map_t map, const char* agent_name, size_t index, const void** data, size_t* len)
{
  if (!map || !agent_name || !data || !len) {
    return NIXL_CAPI_ERROR_INVALID_PARAM;
  }

  try {
    auto it = map->notif_map.find(agent_name);
    if (it == map->notif_map.end() || index >= it->second.size()) {
      return NIXL_CAPI_ERROR_INVALID_PARAM;
    }
    const auto& notif = it->second[index];
    *data = notif.data();
    *len = notif.size();
    return NIXL_CAPI_SUCCESS;
  }
  catch (const std::exception& e) {
    return NIXL_CAPI_ERROR_BACKEND;
  }
}

}  // extern "C"
