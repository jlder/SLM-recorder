// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file sd_files.cpp
 * @brief Authorized, serialized SD file-management requests for web_task.
 *
 * web_task never touches the filesystem directly.  It calls this module, which
 * copies caller arguments into static storage and queues one request for sd_task.
 * sd_task later calls sd_file_ops_service() while SD media access is safe.
 */
#include "src/services/sd_files.h"

#include "src/drivers/sd_storage.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static volatile bool s_authorized = false;
static const TickType_t SD_FILE_WAIT_TICKS = pdMS_TO_TICKS(SD_FILE_OP_TIMEOUT_MS);

static volatile bool s_request_pending = false;
static volatile bool s_request_done = true;
static bool s_request_ok = false;

// One pending operation is encoded with simple flags.  This keeps the SD task
// service hook small and avoids exposing a generic request struct outside this
// module.
static volatile bool s_list_requested = false;
static volatile bool s_space_requested = false;
static volatile bool s_download_begin_requested = false;
static volatile bool s_download_read_requested = false;
static volatile bool s_download_end_requested = false;
static volatile bool s_delete_requested = false;

static volatile bool s_download_active = false;

// Paths and output pointers are copied/stored here because callers may pass
// temporary String buffers.  s_request_done prevents those output pointers from
// being used after the synchronous caller has timed out.
static char s_path[SD_STORAGE_PATH_MAX];
static char *s_out_json = nullptr;
static uint32_t s_out_json_cap = 0u;
static uint8_t *s_out_buf = nullptr;
static uint32_t s_len = 0u;
static uint32_t *s_out_len = nullptr;
static uint32_t *s_out_size = nullptr;
static uint64_t *s_out_total_bytes = nullptr;
static uint64_t *s_out_free_bytes = nullptr;

/** Enable or disable web access to SD file-management operations. */
void sd_files_set_authorized(bool enabled){
  s_authorized = enabled;
}

/** Return true when web access to SD file-management operations is enabled. */
bool sd_files_is_authorized(void){
  return s_authorized;
}

/** Reset the pending request description to its idle state. */
static void request_clear_(void){
  s_list_requested = false;
  s_space_requested = false;
  s_download_begin_requested = false;
  s_download_read_requested = false;
  s_download_end_requested = false;
  s_delete_requested = false;

  s_path[0] = '\0';
  s_out_json = nullptr;
  s_out_json_cap = 0u;
  s_out_buf = nullptr;
  s_len = 0u;
  s_out_len = nullptr;
  s_out_size = nullptr;
  s_out_total_bytes = nullptr;
  s_out_free_bytes = nullptr;
}

/** Copy a path into a fixed request buffer, rejecting null/empty/truncated input. */
static bool copy_path_(char *dst, uint32_t dst_cap, const char *src){
  if((dst == nullptr) || (dst_cap == 0u) || (src == nullptr) || (src[0] == '\0')){
    return false;
  }

  size_t n = 0u;
  while((n < dst_cap) && (src[n] != '\0')){
    ++n;
  }

  if(n >= dst_cap){
    dst[0] = '\0';
    return false;
  }

  memcpy(dst, src, n + 1u);
  return true;
}

/** Reserve the single request slot for a new caller. */
static bool request_begin_(void){
  if(s_request_pending || !s_request_done){
    return false;
  }

  s_request_ok = false;
  s_request_done = false;
  request_clear_();
  return true;
}

/** Cancel a request that could not be fully prepared. */
static void request_cancel_(void){
  request_clear_();
  s_request_ok = false;
  s_request_pending = false;
  s_request_done = true;
}

/**
 * Wait until sd_task has completed the queued request.
 *
 * If the request has not yet been consumed when the timeout expires, it is
 * cancelled.  If sd_task already consumed it, this function keeps waiting so
 * stack-owned output buffers remain valid until sd_task marks the request done.
 */
static bool request_wait_(TickType_t timeout_ticks){
  const TickType_t start = xTaskGetTickCount();
  bool timed_out = false;

  while(!s_request_done){
    if(!timed_out && ((xTaskGetTickCount() - start) >= timeout_ticks)){
      timed_out = true;
      if(s_request_pending){
        s_request_pending = false;
        request_cancel_();
        return false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return timed_out ? false : s_request_ok;
}

/** Queue a directory-list request. */
static bool request_list_(const char *dir_path,
                          char *out_json,
                          uint32_t out_json_cap,
                          uint32_t *out_len){
  if((out_json == nullptr) || (out_json_cap == 0u) || (out_len == nullptr)){
    return false;
  }

  if(!request_begin_()){
    return false;
  }

  if(!copy_path_(s_path, sizeof(s_path), dir_path)){
    request_cancel_();
    return false;
  }

  *out_len = 0u;
  s_out_json = out_json;
  s_out_json_cap = out_json_cap;
  s_out_len = out_len;
  s_list_requested = true;
  s_request_pending = true;
  return true;
}

/** Queue an SD total/free space request. */
static bool request_space_(uint64_t *out_total_bytes, uint64_t *out_free_bytes){
  if((out_total_bytes == nullptr) || (out_free_bytes == nullptr)){
    return false;
  }

  if(!request_begin_()){
    return false;
  }

  *out_total_bytes = 0u;
  *out_free_bytes = 0u;
  s_out_total_bytes = out_total_bytes;
  s_out_free_bytes = out_free_bytes;
  s_space_requested = true;
  s_request_pending = true;
  return true;
}

/** Queue a file archive request. */
static bool request_delete_(const char *path){
  if(!request_begin_()){
    return false;
  }

  if(!copy_path_(s_path, sizeof(s_path), path)){
    request_cancel_();
    return false;
  }

  s_delete_requested = true;
  s_request_pending = true;
  return true;
}

/** Queue opening of a sequential download session. */
static bool request_download_begin_(const char *path, uint32_t *out_size){
  if((path == nullptr) || (out_size == nullptr)){
    return false;
  }

  if(!request_begin_()){
    return false;
  }

  if(!copy_path_(s_path, sizeof(s_path), path)){
    request_cancel_();
    return false;
  }

  s_out_size = out_size;
  s_download_begin_requested = true;
  s_request_pending = true;
  return true;
}

/** Queue reading of the next sequential download chunk. */
static bool request_download_read_(uint8_t *out, uint32_t len, uint32_t *out_len){
  if((out == nullptr) || (out_len == nullptr) || (len == 0u)){
    return false;
  }

  if(!request_begin_()){
    return false;
  }

  s_out_buf = out;
  s_len = len;
  s_out_len = out_len;
  s_download_read_requested = true;
  s_request_pending = true;
  return true;
}

/** Queue closing of the active sequential download session. */
static bool request_download_end_(void){
  if(!request_begin_()){
    return false;
  }

  s_download_end_requested = true;
  s_request_pending = true;
  return true;
}

/** List root recording files as compact JSON. */
bool sd_files_list_json(const char *dir_path,
                        char *out_json,
                        uint32_t out_json_cap,
                        uint32_t *out_len){
  if(!sd_files_is_authorized()){
    return false;
  }

  if(!request_list_(dir_path, out_json, out_json_cap, out_len)){
    return false;
  }

  return request_wait_(SD_FILE_WAIT_TICKS);
}

/** Query SD total/free bytes from sd_task context. */
bool sd_files_get_space(uint64_t *out_total_bytes, uint64_t *out_free_bytes){
  if(!sd_files_is_authorized()){
    return false;
  }

  if(!request_space_(out_total_bytes, out_free_bytes)){
    return false;
  }

  return request_wait_(SD_FILE_WAIT_TICKS);
}

/** Begin an SD-task-owned sequential download session. */
bool sd_files_download_begin(const char *path, uint32_t *out_size){
  if(!sd_files_is_authorized()){
    return false;
  }

  if(!request_download_begin_(path, out_size)){
    return false;
  }

  const bool ok = request_wait_(SD_FILE_WAIT_TICKS);
  s_download_active = ok;
  return ok;
}

/** Read the next chunk from the active sequential download session. */
bool sd_files_download_read(uint8_t *out, uint32_t len, uint32_t *out_len){
  if(!sd_files_is_authorized() || !s_download_active){
    return false;
  }

  if(!request_download_read_(out, len, out_len)){
    return false;
  }

  return request_wait_(SD_FILE_WAIT_TICKS);
}

/** End the active sequential download session. */
bool sd_files_download_end(void){
  if(!s_download_active){
    return true;
  }

  if(!request_download_end_()){
    return false;
  }

  const bool ok = request_wait_(SD_FILE_WAIT_TICKS);
  s_download_active = false;
  return ok;
}

/** Return true while a web download session is open or being serviced. */
bool sd_files_download_active(void){
  return s_download_active;
}

/** Archive a root-level file to /processed. */
bool sd_files_delete(const char *path){
  if(!sd_files_is_authorized()){
    return false;
  }

  if(!request_delete_(path)){
    return false;
  }

  return request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * Execute one queued web-file request in sd_task context.
 *
 * sd_task calls this only while idle, so these direct sd_storage_* calls remain
 * serialized with recorder open/write/close operations.
 */
void sd_file_ops_service(void){
  if(!s_request_pending){
    return;
  }

  s_request_pending = false;
  s_request_ok = false;

  if(s_delete_requested){
    s_delete_requested = false;
    s_request_ok = sd_storage_archive_to_processed(s_path);

  } else if(s_space_requested){
    s_space_requested = false;
    if((s_out_total_bytes != nullptr) && (s_out_free_bytes != nullptr)){
      *s_out_total_bytes = sd_storage_total_bytes_get();
      *s_out_free_bytes = sd_get_free_bytes();
      s_request_ok = (*s_out_total_bytes > 0u);
    }

  } else if(s_download_begin_requested){
    s_download_begin_requested = false;
    s_request_ok = sd_storage_download_begin(s_path, s_out_size);

  } else if(s_download_read_requested){
    s_download_read_requested = false;
    s_request_ok = sd_storage_download_read(s_out_buf, s_len, s_out_len);

  } else if(s_download_end_requested){
    s_download_end_requested = false;
    sd_storage_download_end();
    s_request_ok = true;

  } else if(s_list_requested){
    s_list_requested = false;
    s_request_ok = sd_storage_list_json(s_path, s_out_json, s_out_json_cap, s_out_len);
  }

  s_request_done = true;
}
