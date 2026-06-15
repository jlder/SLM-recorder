// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/sd_storage.h
 * @brief Public raw SD storage API used by the SD task and SD file-service layer.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "src/services/error_manager.h"

typedef enum {
  SD_STATUS_PRESENCE_SPACE = 0,
  SD_STATUS_ALL
} sd_status_scope_t;


/**
 * @brief SD begin.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_begin(void);
/**
 * @brief SD end.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_end(void);
/**
 * @brief SD reinit.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_reinit(void);

/**
 * @brief SD storage is open.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_is_open(void);


/**
 * @brief SD status check.
 *
 * Inputs: `scope`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_status_check(sd_status_scope_t scope);
/**
 * @brief SD storage total bytes get.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint64_t sd_storage_total_bytes_get(void);
/**
 * @brief SD get free bytes.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint64_t sd_get_free_bytes(void);

/**
 * @brief SD open record.
 *
 * Inputs: `path`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_open_record(const char *path);

/**
 * Open the daily recording file and append a new recording session.
 *
 * Expected root-directory policy: for one registration/date prefix there shall
 * be either zero or one matching file.  If the file exists, its _N suffix is
 * incremented before the new session is appended.  More than one matching file
 * is treated as an SD file-management inconsistency.
 *
 * Inputs: `prefix`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_open_record_daily(const char *prefix);
/**
 * @brief SD write record block.
 *
 * Inputs: `data`, `len`, `out_written`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_write_record_block(const uint8_t *data, size_t len, size_t *out_written);
/**
 * @brief SD flush record.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_flush_record(void);
/**
 * @brief SD close record.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_close_record(void);

/**
 * @brief SD storage list JSON.
 *
 * Inputs: `dir_path`, `out_json`, `out_json_cap`, `out_len`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_list_json(const char *dir_path, char *out_json, uint32_t out_json_cap, uint32_t *out_len);
/**
 * @brief SD storage get file size.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_get_file_size(const char *path, uint32_t *out_size);
/**
 * @brief SD storage read.
 *
 * Inputs: `path`, `offset`, `len`, `out`, `out_len`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_read(const char *path, uint32_t offset, uint32_t len, uint8_t *out, uint32_t *out_len);
/**
 * Begin an SD-owned sequential download session.  The file remains open until
 * sd_storage_download_end() is called.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the file was opened and its size returned.
 */
bool sd_storage_download_begin(const char *path, uint32_t *out_size);

/**
 * Read the next sequential chunk from the active download session.
 *
 * Inputs: `out`, `len`, `out_len`.
 * Returns: `true` when the read operation completed.
 */
bool sd_storage_download_read(uint8_t *out, uint32_t len, uint32_t *out_len);

/**
 * End the active SD-owned download session and close the file handle.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_storage_download_end(void);

/**
 * Return whether an SD-owned download file handle is currently open.
 *
 * Inputs: None.
 * Returns: `true` when a download session is active.
 */
bool sd_storage_download_active(void);
/**
 * @brief SD storage delete.
 *
 * Inputs: `path`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_delete(const char *path);

/**
 * Archive a root-level file by moving it into /processed.  If the destination
 * name already exists, append _N before the extension.
 *
 * Inputs: `path`.
 * Returns: `true` when the file was moved to /processed; otherwise `false`.
 */
bool sd_storage_archive_to_processed(const char *path);
/**
 * @brief SD storage move.
 *
 * Inputs: `src`, `dst`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_move(const char *src, const char *dst);
