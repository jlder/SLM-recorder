// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file sd_files.h
 * @brief Authorized web-facing SD file-management API.
 *
 * @details
 * web_task calls sd_files_* functions only when it needs to list, download,
 * archive, or move recorded files.  The functions below check the authorization
 * flag owned by state_task, then submit one serialized file request for the SD
 * task state machine to execute.  Raw filesystem access remains in sd_storage.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Set SD file-management authorization (controlled by state_task). */
void sd_files_set_authorized(bool enabled);

/** Query SD file-management authorization. */
bool sd_files_is_authorized(void);

/**
 * List directory entries as a compact JSON array.
 * Returns false if not authorized, busy, timed out, or on SD/filesystem error.
 */
bool sd_files_list_json(const char *dir_path,
                        char *out_json,
                        uint32_t out_json_cap,
                        uint32_t *out_len);

/**
 * Return the size of a regular file.
 * Returns false for unauthorized access, missing paths, directories, busy,
 * timeout, or SD/filesystem error.
 */
bool sd_files_get_file_size(const char *path, uint32_t *out_size);

/**
 * Return SD total/free space by executing the query in SD task context.
 * Returns false if not authorized, busy, timed out, or SD media is unavailable.
 */
bool sd_files_get_space(uint64_t *out_total_bytes, uint64_t *out_free_bytes);

/** Read a chunk from a regular file. Returns false if not authorized or on error. */
bool sd_files_read(const char *path, uint32_t offset, uint32_t len,
                   uint8_t *out, uint32_t *out_len);

/** Archive a file to /processed. Returns false if not authorized or on error. */
bool sd_files_delete(const char *path);

/** Move/rename a file. Returns false if not authorized or on error. */
bool sd_files_move(const char *src, const char *dst);

/**
 * SD-task service hook.
 * Called frequently from the SD state machine while it is safe to perform
 * file-management operations.
 */
void sd_file_ops_service(void);

#ifdef __cplusplus
} // extern "C"
#endif
