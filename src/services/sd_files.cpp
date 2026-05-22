// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file sd_files.cpp
 * @brief Authorization gate plus serialized file requests for the SD task.
 *
 * Design intent:
 * - state_task controls authorization (READY + WiFi enabled).
 * - web_task calls only sd_files_* functions.
 * - sd_task calls sd_file_ops_service() from the SD state machine.
 * - sd_storage_* remains the only layer touching the SD filesystem.
 */
#include "src/services/sd_files.h"

#include "src/drivers/sd_storage.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static volatile bool s_authorized = false;

// Keep path storage local to this module so callers may pass temporary
// String::c_str() pointers safely; the SD task consumes copied paths.
static const TickType_t SD_FILE_WAIT_TICKS = pdMS_TO_TICKS(SD_FILE_OP_TIMEOUT_MS);

static volatile bool s_file_request_pending = false;
static volatile bool s_file_request_done = true;
static bool s_file_request_ok = false;

// Only one request is allowed at a time.  These flags identify the pending
// operation without exposing a generic request enum/struct API.
static volatile bool s_file_list_json_requested = false;
static volatile bool s_file_size_requested = false;
static volatile bool s_file_space_requested = false;
static volatile bool s_file_read_requested = false;
static volatile bool s_file_download_begin_requested = false;
static volatile bool s_file_download_read_requested = false;
static volatile bool s_file_download_end_requested = false;
static volatile bool s_file_delete_requested = false;
static volatile bool s_file_move_requested = false;

static volatile bool s_file_download_active = false;

static char s_file_path[SD_STORAGE_PATH_MAX];
static char s_file_path2[SD_STORAGE_PATH_MAX];

static char *s_file_out_json = nullptr;
static uint32_t s_file_out_json_cap = 0u;

static uint8_t *s_file_out_buf = nullptr;
static uint32_t s_file_offset = 0u;
static uint32_t s_file_len = 0u;

static uint32_t *s_file_out_len = nullptr;
static uint32_t *s_file_out_size = nullptr;
static uint64_t *s_file_out_total_bytes = nullptr;
static uint64_t *s_file_out_free_bytes = nullptr;

/**
 * Updates sd files set authorized state and applies the change to the owning
 * module or hardware interface.
 *
 * Inputs: `enabled`.
 * Returns: None.
 */
void sd_files_set_authorized(bool enabled){
  s_authorized = enabled;
}

/**
 * Performs sd files is authorized for SD storage, recording files, or SD-
 * backed web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_files_is_authorized(void){
  return s_authorized;
}

/**
 * Performs sd files authorized for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_files_authorized_(void){
  return sd_files_is_authorized();
}

/**
 * Clears sd file request clear args state, latches, or stored data so the
 * owning module returns to its idle/default condition.
 *
 * Inputs: None.
 * Returns: None.
 */
static void sd_file_request_clear_args_(void){
  s_file_list_json_requested = false;
  s_file_size_requested = false;
  s_file_space_requested = false;
  s_file_read_requested = false;
  s_file_download_begin_requested = false;
  s_file_download_read_requested = false;
  s_file_download_end_requested = false;
  s_file_delete_requested = false;
  s_file_move_requested = false;

  s_file_path[0] = '\0';
  s_file_path2[0] = '\0';

  s_file_out_json = nullptr;
  s_file_out_json_cap = 0u;
  s_file_out_buf = nullptr;
  s_file_offset = 0u;
  s_file_len = 0u;
  s_file_out_len = nullptr;
  s_file_out_size = nullptr;
  s_file_out_total_bytes = nullptr;
  s_file_out_free_bytes = nullptr;
}

/**
 * Performs sd file copy path for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `dst`, `dst_cap`, `src`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_file_copy_path_(char *dst, uint32_t dst_cap, const char *src){
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

/**
 * Performs sd file request begin for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_file_request_begin_(void){
  // The web layer currently serializes SD endpoints.  This guard also protects
  // against accidental concurrent callers or a previous request still being
  // processed by the SD task.
  if(s_file_request_pending || !s_file_request_done){
    return false;
  }

  s_file_request_ok = false;
  s_file_request_done = false;
  sd_file_request_clear_args_();
  return true;
}

/**
 * Performs sd file request cancel begin for SD storage, recording files, or
 * SD-backed web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
static void sd_file_request_cancel_begin_(void){
  sd_file_request_clear_args_();
  s_file_request_ok = false;
  s_file_request_pending = false;
  s_file_request_done = true;
}

/**
 * Performs sd file request wait for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `timeout_ticks`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_file_request_wait_(TickType_t timeout_ticks){
  const TickType_t start = xTaskGetTickCount();
  bool timed_out = false;

  while(!s_file_request_done){
    if(!timed_out && ((xTaskGetTickCount() - start) >= timeout_ticks)){
      timed_out = true;

      // If the SD task has not consumed the request yet, cancel it so stale
      // caller-owned output buffers are not used later.
      if(s_file_request_pending){
        s_file_request_pending = false;
        sd_file_request_cancel_begin_();
        return false;
      }

      // If the SD task already consumed the request, keep waiting until it
      // marks done. Returning now could let stack-owned output buffers expire
      // while the SD task is still using them.
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return timed_out ? false : s_file_request_ok;
}

/**
 * Performs sd request file list json for SD storage, recording files, or SD-
 * backed web file management while preserving SD ownership rules.
 *
 * Inputs: `dir_path`, `out_json`, `out_json_cap`, `out_len`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static bool sd_request_file_list_json_(const char *dir_path,
                                       char *out_json,
                                       uint32_t out_json_cap,
                                       uint32_t *out_len){
  if((out_json == nullptr) || (out_json_cap == 0u) || (out_len == nullptr)){
    return false;
  }

  if(!sd_file_request_begin_()){
    return false;
  }

  if(!sd_file_copy_path_(s_file_path, sizeof(s_file_path), dir_path)){
    sd_file_request_cancel_begin_();
    return false;
  }

  *out_len = 0u;
  s_file_out_json = out_json;
  s_file_out_json_cap = out_json_cap;
  s_file_out_len = out_len;
  s_file_list_json_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Returns the requested sd request file get size information from the module
 * state or underlying driver interface.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_request_file_get_size_(const char *path, uint32_t *out_size){
  if(out_size == nullptr){
    return false;
  }

  if(!sd_file_request_begin_()){
    return false;
  }

  if(!sd_file_copy_path_(s_file_path, sizeof(s_file_path), path)){
    sd_file_request_cancel_begin_();
    return false;
  }

  *out_size = 0u;
  s_file_out_size = out_size;
  s_file_size_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Performs sd request file space for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `out_total_bytes`, `out_free_bytes`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_request_file_space_(uint64_t *out_total_bytes, uint64_t *out_free_bytes){
  if((out_total_bytes == nullptr) || (out_free_bytes == nullptr)){
    return false;
  }

  if(!sd_file_request_begin_()){
    return false;
  }

  *out_total_bytes = 0u;
  *out_free_bytes = 0u;
  s_file_out_total_bytes = out_total_bytes;
  s_file_out_free_bytes = out_free_bytes;
  s_file_space_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Performs sd request file read for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `path`, `offset`, `len`, `out`, `out_len`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static bool sd_request_file_read_(const char *path,
                                  uint32_t offset,
                                  uint32_t len,
                                  uint8_t *out,
                                  uint32_t *out_len){
  if((out == nullptr) || (out_len == nullptr)){
    return false;
  }

  if(!sd_file_request_begin_()){
    return false;
  }

  if(!sd_file_copy_path_(s_file_path, sizeof(s_file_path), path)){
    sd_file_request_cancel_begin_();
    return false;
  }

  *out_len = 0u;
  s_file_offset = offset;
  s_file_len = len;
  s_file_out_buf = out;
  s_file_out_len = out_len;
  s_file_read_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Performs sd request file delete for SD storage, recording files, or SD-
 * backed web file management while preserving SD ownership rules.
 *
 * Inputs: `path`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_request_file_delete_(const char *path){
  if(!sd_file_request_begin_()){
    return false;
  }

  if(!sd_file_copy_path_(s_file_path, sizeof(s_file_path), path)){
    sd_file_request_cancel_begin_();
    return false;
  }

  s_file_delete_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Performs sd request file move for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `src`, `dst`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_request_file_move_(const char *src, const char *dst){
  if(!sd_file_request_begin_()){
    return false;
  }

  if(!sd_file_copy_path_(s_file_path, sizeof(s_file_path), src) ||
     !sd_file_copy_path_(s_file_path2, sizeof(s_file_path2), dst)){
    sd_file_request_cancel_begin_();
    return false;
  }

  s_file_move_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Performs sd files list json for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `dir_path`, `out_json`, `out_json_cap`, `out_len`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */

/**
 * Requests that sd_task open a sequential download session.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the request was queued.
 */
static bool sd_request_download_begin_(const char *path, uint32_t *out_size){
  if((path == nullptr) || (out_size == nullptr)){
    return false;
  }

  if(!sd_file_request_begin_()){
    return false;
  }

  if(!sd_file_copy_path_(s_file_path, sizeof(s_file_path), path)){
    sd_file_request_cancel_begin_();
    return false;
  }

  s_file_out_size = out_size;
  s_file_download_begin_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Requests that sd_task read the next sequential download chunk.
 *
 * Inputs: `out`, `len`, `out_len`.
 * Returns: `true` when the request was queued.
 */
static bool sd_request_download_read_(uint8_t *out, uint32_t len, uint32_t *out_len){
  if((out == nullptr) || (out_len == nullptr) || (len == 0u)){
    return false;
  }

  if(!sd_file_request_begin_()){
    return false;
  }

  s_file_out_buf = out;
  s_file_len = len;
  s_file_out_len = out_len;
  s_file_download_read_requested = true;
  s_file_request_pending = true;
  return true;
}

/**
 * Requests that sd_task close the active sequential download session.
 *
 * Inputs: None.
 * Returns: `true` when the request was queued.
 */
static bool sd_request_download_end_(void){
  if(!sd_file_request_begin_()){
    return false;
  }

  s_file_download_end_requested = true;
  s_file_request_pending = true;
  return true;
}

bool sd_files_list_json(const char *dir_path,
                        char *out_json,
                        uint32_t out_json_cap,
                        uint32_t *out_len){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_file_list_json_(dir_path, out_json, out_json_cap, out_len)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * Returns the requested sd files get file size information from the module
 * state or underlying driver interface.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_files_get_file_size(const char *path, uint32_t *out_size){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_file_get_size_(path, out_size)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * Returns the requested sd files get space information from the module state
 * or underlying driver interface.
 *
 * Inputs: `out_total_bytes`, `out_free_bytes`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_files_get_space(uint64_t *out_total_bytes, uint64_t *out_free_bytes){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_file_space_(out_total_bytes, out_free_bytes)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * Performs sd files read for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `path`, `offset`, `len`, `out`, `out_len`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool sd_files_read(const char *path,
                   uint32_t offset,
                   uint32_t len,
                   uint8_t *out,
                   uint32_t *out_len){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_file_read_(path, offset, len, out, out_len)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}


/**
 * Begin an SD-task-owned sequential download session.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the session is open and size is known.
 */
bool sd_files_download_begin(const char *path, uint32_t *out_size){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_download_begin_(path, out_size)){
    return false;
  }

  const bool ok = sd_file_request_wait_(SD_FILE_WAIT_TICKS);
  s_file_download_active = ok;
  return ok;
}

/**
 * Read the next chunk from the active SD-task-owned download session.
 *
 * Inputs: `out`, `len`, `out_len`.
 * Returns: `true` when the read completed.
 */
bool sd_files_download_read(uint8_t *out, uint32_t len, uint32_t *out_len){
  if(!sd_files_authorized_() || !s_file_download_active){
    return false;
  }

  if(!sd_request_download_read_(out, len, out_len)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * End the active SD-task-owned download session.
 *
 * Inputs: None.
 * Returns: `true` when the close request completed or no session was active.
 */
bool sd_files_download_end(void){
  if(!s_file_download_active){
    return true;
  }

  if(!sd_request_download_end_()){
    return false;
  }

  const bool ok = sd_file_request_wait_(SD_FILE_WAIT_TICKS);
  s_file_download_active = false;
  return ok;
}

/**
 * Return whether a download session is active or being opened/closed.
 *
 * Inputs: None.
 * Returns: `true` when a download session is active.
 */
bool sd_files_download_active(void){
  return s_file_download_active;
}

/**
 * Performs sd files delete for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `path`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_files_delete(const char *path){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_file_delete_(path)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * Performs sd files move for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `src`, `dst`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_files_move(const char *src, const char *dst){
  if(!sd_files_authorized_()){
    return false;
  }

  if(!sd_request_file_move_(src, dst)){
    return false;
  }

  return sd_file_request_wait_(SD_FILE_WAIT_TICKS);
}

/**
 * Performs sd file ops service for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_file_ops_service(void){
  if(!s_file_request_pending){
    return;
  }

  s_file_request_pending = false;
  s_file_request_ok = false;

  // sd_task calls this service only while idle, so the direct sd_storage_*
  // calls below remain serialized with recorder open/write/close operations.
  if(s_file_delete_requested){
    s_file_delete_requested = false;
    s_file_request_ok = sd_storage_archive_to_processed(s_file_path);

  } else if(s_file_move_requested){
    s_file_move_requested = false;
    s_file_request_ok = sd_storage_move(s_file_path, s_file_path2);

  } else if(s_file_size_requested){
    s_file_size_requested = false;
    s_file_request_ok = sd_storage_get_file_size(s_file_path, s_file_out_size);

  } else if(s_file_space_requested){
    s_file_space_requested = false;
    if((s_file_out_total_bytes != nullptr) && (s_file_out_free_bytes != nullptr)){
      *s_file_out_total_bytes = sd_storage_total_bytes_get();
      *s_file_out_free_bytes = sd_get_free_bytes();
      s_file_request_ok = (*s_file_out_total_bytes > 0u);
    }

  } else if(s_file_read_requested){
    s_file_read_requested = false;
    s_file_request_ok = sd_storage_read(
        s_file_path,
        s_file_offset,
        s_file_len,
        s_file_out_buf,
        s_file_out_len);

  } else if(s_file_download_begin_requested){
    s_file_download_begin_requested = false;
    s_file_request_ok = sd_storage_download_begin(s_file_path, s_file_out_size);

  } else if(s_file_download_read_requested){
    s_file_download_read_requested = false;
    s_file_request_ok = sd_storage_download_read(
        s_file_out_buf,
        s_file_len,
        s_file_out_len);

  } else if(s_file_download_end_requested){
    s_file_download_end_requested = false;
    sd_storage_download_end();
    s_file_request_ok = true;

  } else if(s_file_list_json_requested){
    s_file_list_json_requested = false;
    s_file_request_ok = sd_storage_list_json(
        s_file_path,
        s_file_out_json,
        s_file_out_json_cap,
        s_file_out_len);
  }

  s_file_request_done = true;
}
