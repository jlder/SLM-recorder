// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/sd_storage.cpp
 * @brief Raw SD/MMC storage implementation for recorder writes and file-management operations.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/sd_storage.h"

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "mbedtls/sha256.h"

// The ESP32 Arduino core used by this recorder exposes the mbedTLS 2.x
// SHA-256 API without the newer `_ret` suffix.  These wrappers intentionally
// ignore the legacy functions' return type (void in this toolchain) and keep
// the recorder code independent of that API naming difference.
static bool sd_sha256_starts_(mbedtls_sha256_context *ctx) {
  mbedtls_sha256_starts(ctx, 0);
  return true;
}

static bool sd_sha256_update_(mbedtls_sha256_context *ctx,
                              const uint8_t *data,
                              size_t length) {
  mbedtls_sha256_update(ctx, data, length);
  return true;
}

static bool sd_sha256_finish_(mbedtls_sha256_context *ctx, uint8_t digest[32]) {
  mbedtls_sha256_finish(ctx, digest);
  return true;
}

#include "sdmmc_cmd.h"
#include "src/board/pin_config.h"
#include "config.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Free-space threshold required before a recording can start.
static constexpr uint64_t SD_RECORD_START_MIN_FREE_BYTES =
    (uint64_t)SD_RECORD_START_MIN_FREE_MB * 1024ULL * 1024ULL;

// Lower free-space threshold used while recording is already active. The gap
// between start and record-low thresholds prevents immediate low-space closure
// when recording starts close to the start threshold.
static constexpr uint64_t SD_RECORD_LOW_FREE_BYTES =
    (uint64_t)SD_RECORD_LOW_FREE_MB * 1024ULL * 1024ULL;

// True after a successful mount / detect sequence.
static bool s_mounted = false;

// Cached low-level card handle needed for sdmmc_get_status().
static sdmmc_card_t *s_card = nullptr;

// Current open recording file, if any.
static File s_file;
static char s_record_path[SD_STORAGE_PATH_MAX] = {};
static mbedtls_sha256_context s_record_sha_ctx;
static bool s_record_sha_active = false;
static uint64_t s_record_sha_size = 0u;

// Current open Web/UI download file, if any.  This handle is owned by the
// SD layer and is serviced only from sd_task through sd_files requests.
static File s_download_file;
static uint32_t s_download_size = 0u;
static uint32_t s_download_offset = 0u;

// Cached free-space value used only while a record file is open.
// This avoids querying the SD stack for free space during active writes.
static uint64_t s_cached_free_bytes = 0u;

// Distinguishes a valid cached zero free-space value from "cache unavailable".
static bool s_cached_free_valid = false;

// Accessor used to reach the lower-level card pointer held by SD_MMC.
// This is intentionally kept because sdmmc_get_status() is the reliable way to
// detect whether the card remains accessible.
struct SDMMCFSAccessor : public fs::SDMMCFS {
  using fs::SDMMCFS::_card;
};

/**
 * @brief Return the low-level SD/MMC card handle.
 *
 * Inputs: None.
 * Returns: Pointer to the mounted card object, or `nullptr` if unavailable.
 */
static sdmmc_card_t *sd_get_card(void) {
  return reinterpret_cast<SDMMCFSAccessor*>(&SD_MMC)->_card;
}

/**
 * Performs sd reset runtime state for SD storage, recording files, or SD-
 * backed web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
static void sd_reset_runtime_state_(void) {
  s_mounted = false;
  s_card = nullptr;
  s_cached_free_bytes = 0u;
  s_cached_free_valid = false;
  s_record_path[0] = '\0';
  s_record_sha_active = false;
  s_record_sha_size = 0u;
}

/**
 * Performs sd detect mount state for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
static error_code_t sd_detect_mount_state_(void) {
  s_card = sd_get_card();
  s_mounted = (s_card != nullptr) && (SD_MMC.cardType() != CARD_NONE);

  // A mount-state transition invalidates any previously cached free-space value.
  s_cached_free_valid = false;
  return s_mounted ? ERR_NONE : ERR_SD_NO_CARD;
}

typedef struct {
  char name[FILENAME_MAX_LENGTH];
  uint32_t size;
} sd_storage_list_item_t;

/** Return true when a path or file name ends with the supplied extension. */
static bool sd_name_has_extension_(const char *name, const char *ext){
  if((name == nullptr) || (ext == nullptr)){
    return false;
  }

  const size_t name_len = strlen(name);
  const size_t ext_len = strlen(ext);
  if((ext_len == 0u) || (name_len < ext_len)){
    return false;
  }

  return strcmp(name + name_len - ext_len, ext) == 0;
}

/**
 * Performs sd norm sdmmc path for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `in`, `out`, `out_sz`.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
static const char* sd_norm_sdmmc_path_(const char *in, char *out, size_t out_sz){
  if(!in || !out || out_sz == 0) return nullptr;
  const char *p = in;
  if(strncmp(p, "/sdcard", 7) == 0) p += 7;
  if(p[0] == '\0') p = "/";
  if(p[0] != '/'){
    (void)snprintf(out, out_sz, "/%s", p);
    return out;
  }
  return p;
}

/**
 * Performs sd card access ok for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_ensure_calibration_reports_dir_(void);
static bool sd_path_is_root_file_(const char *p);

static bool sd_card_access_ok(void) {
  if (SD_MMC.cardType() == CARD_NONE) {
    return false;
  }

  if (s_card == nullptr) {
    return false;
  }

  return (sdmmc_get_status(s_card) == ESP_OK);
}

// Classify a failed SD I/O operation into the user-facing error model.
// The split is intentionally based on what the user can do to recover:
// - ERR_SD_NO_CARD: media unavailable (not mounted, removed, or inaccessible)
// - ERR_SD_FAULT: unexpected I/O failure while media still appears present
/**
 * Performs sd classify io fault for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
static error_code_t sd_classify_io_fault(void) {
  if (!s_mounted) {
    return ERR_SD_NO_CARD;
  }

  if (!sd_card_access_ok()) {
    return ERR_SD_NO_CARD;
  }

  return ERR_SD_FAULT;
}

/**
 * Performs sd begin for SD storage, recording files, or SD-backed web file
 * management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
static error_code_t sd_begin_(void) {
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);

  if (!SD_MMC.begin("/sdcard", true)) {
    sd_reset_runtime_state_();
    return ERR_SD_NO_CARD;
  }

  return sd_detect_mount_state_();
}

/**
 * Performs sd end for SD storage, recording files, or SD-backed web file
 * management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_end(void) {
  if (s_download_file) {
    s_download_file.close();
    s_download_file = File();
    s_download_size = 0u;
    s_download_offset = 0u;
  }

  if (s_file) {
    s_file.flush();
    s_file.close();
    s_file = File();
  }

  SD_MMC.end();
  sd_reset_runtime_state_();
}

/**
 * Performs sd reinit for SD storage, recording files, or SD-backed web file
 * management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_reinit(void) {
  sd_end();
  return sd_begin_();
}

/**
 * Performs sd storage is open for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_is_open(void) {
  return (bool)s_file;
}

/**
 * Performs sd check present for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
static error_code_t sd_check_present(void) {
  if (!s_mounted) {
    return ERR_SD_NO_CARD;
  }

  if (!sd_card_access_ok()) {
    return ERR_SD_NO_CARD;
  }

  return ERR_NONE;
}

/**
 * Performs sd free bytes for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `out_free_bytes`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
static error_code_t sd_free_bytes(uint64_t *out_free_bytes) {
  const error_code_t rc = sd_check_present();
  if (rc != ERR_NONE) {
    return rc;
  }

  const uint64_t total = SD_MMC.totalBytes();
  const uint64_t used = SD_MMC.usedBytes();
  const uint64_t free_bytes = (total > used) ? (total - used) : 0u;

  *out_free_bytes = free_bytes;
  return ERR_NONE;
}

/**
 * Performs sd root files full for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `out_full`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
static error_code_t sd_root_files_full(bool *out_full) {
  const error_code_t rc = sd_check_present();
  if (rc != ERR_NONE) {
    return rc;
  }

  File root = SD_MMC.open("/");
  if (!root) {
    return sd_classify_io_fault();
  }

  size_t count = 0u;
  for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    if ((!entry.isDirectory()) && sd_name_has_extension_(entry.name(), ".bin")) {
      ++count;
      if (count >= (size_t)SD_MAX_RECORD_FILES) {
        entry.close();
        root.close();
        *out_full = true;
        return ERR_NONE;
      }
    }
    entry.close();
  }

  root.close();
  *out_full = false;
  return ERR_NONE;
}

/**
 * Performs sd status check for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `scope`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_status_check(sd_status_scope_t scope) {
  if (!s_mounted) {
    return ERR_SD_NO_CARD;
  }

  const error_code_t present_rc = sd_check_present();
  if (present_rc != ERR_NONE) {
    return present_rc;
  }

  const uint64_t free_bytes = sd_get_free_bytes();

  if (scope == SD_STATUS_ALL) {
    bool files_full = false;
    const error_code_t files_rc = sd_root_files_full(&files_full);
    if (files_rc != ERR_NONE) {
      return files_rc;
    }
    if (files_full) {
      return ERR_SD_FILES_FULL;
    }
  }

  if (free_bytes < SD_RECORD_START_MIN_FREE_BYTES) {
    return ERR_SD_SPACE_LOW;
  }

  return ERR_NONE;
}

/**
 * Returns the requested sd storage total bytes get information from the module
 * state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint64_t sd_storage_total_bytes_get(void) {
  if (sd_check_present() != ERR_NONE) {
    return 0u;
  }
  return SD_MMC.totalBytes();
}

/**
 * Returns the requested sd get free bytes information from the module state or
 * underlying driver interface.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint64_t sd_get_free_bytes(void) {
  if (sd_storage_is_open() && s_cached_free_valid) {
    return s_cached_free_bytes;
  }

  uint64_t free_bytes = 0u;
  if (sd_free_bytes(&free_bytes) != ERR_NONE) {
    return 0u;
  }

  return free_bytes;
}


static bool sd_sha_path_build_(const char *bin_path, char *out, size_t out_cap) {
  if((bin_path == nullptr) || (out == nullptr) || (out_cap == 0u)) return false;
  const size_t n = strlen(bin_path);
  if((n < 4u) || (strcmp(bin_path + n - 4u, ".bin") != 0)) return false;
  if(n + 1u > out_cap) return false;
  memcpy(out, bin_path, n - 4u);
  const int written = snprintf(out + n - 4u, out_cap - (n - 4u), ".sha");
  return (written == 4);
}

static const char *sd_basename_local_(const char *path) {
  const char *slash = path ? strrchr(path, '/') : nullptr;
  return slash ? slash + 1 : path;
}

static bool sd_sha_write_metadata_(const char *bin_path,
                                   uint64_t file_size,
                                   const uint8_t digest[32]) {
  char sha_path[SD_STORAGE_PATH_MAX];
  if(!sd_sha_path_build_(bin_path, sha_path, sizeof(sha_path))) return false;

  char hex[65];
  for(size_t i = 0u; i < 32u; ++i) {
    snprintf(hex + (i * 2u), 3u, "%02x", digest[i]);
  }
  hex[64] = '\0';

  char text[256];
  const int n = snprintf(text, sizeof(text),
                         "format=1\nfilename=%s\nsize=%llu\nsha256=%s\n",
                         sd_basename_local_(bin_path),
                         (unsigned long long)file_size,
                         hex);
  if((n <= 0) || ((size_t)n >= sizeof(text))) return false;

  if(SD_MMC.exists(sha_path)) {
    (void)SD_MMC.remove(sha_path);
  }
  File meta = SD_MMC.open(sha_path, FILE_WRITE);
  if(!meta) return false;
  const size_t written = meta.write((const uint8_t *)text, (size_t)n);
  meta.flush();
  meta.close();
  return written == (size_t)n;
}

static bool sd_sha256_file_(const char *path,
                            uint8_t digest[32],
                            uint64_t *out_size,
                            sd_sha_progress_cb_t progress_cb) {
  File file = SD_MMC.open(path, FILE_READ);
  if(!file || file.isDirectory()) {
    if(file) file.close();
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  if(!sd_sha256_starts_(&ctx)) {
    mbedtls_sha256_free(&ctx);
    file.close();
    return false;
  }

  uint8_t buffer[1024];
  uint64_t total = 0u;
  bool ok = true;
  while(file.available()) {
    const int count = file.read(buffer, sizeof(buffer));
    if(count <= 0) { ok = false; break; }
    if(!sd_sha256_update_(&ctx, buffer, (size_t)count)) {
      ok = false;
      break;
    }
    total += (uint64_t)count;
    if(progress_cb != nullptr) progress_cb();
  }

  if(ok) ok = sd_sha256_finish_(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  file.close();
  if(out_size != nullptr) *out_size = total;
  return ok;
}

static bool sd_sha_parse_metadata_(const char *sha_path,
                                   const char *expected_filename,
                                   uint64_t *out_size,
                                   uint8_t digest[32]) {
  File file = SD_MMC.open(sha_path, FILE_READ);
  if(!file || file.isDirectory()) {
    if(file) file.close();
    return false;
  }
  if(file.size() >= 256u) { file.close(); return false; }

  char text[256];
  const size_t n = file.readBytes(text, sizeof(text) - 1u);
  file.close();
  text[n] = '\0';

  char filename[FILENAME_MAX_LENGTH] = {};
  char sha_hex[65] = {};
  unsigned long long size_value = 0u;
  int format = 0;
  const int matched = sscanf(text,
                             "format=%d\nfilename=%63[^\n]\nsize=%llu\nsha256=%64[0-9a-fA-F]",
                             &format, filename, &size_value, sha_hex);
  if((matched != 4) || (format != 1) ||
     (strcmp(filename, expected_filename) != 0) ||
     (strlen(sha_hex) != 64u)) return false;

  for(size_t i = 0u; i < 32u; ++i) {
    unsigned int value = 0u;
    if(sscanf(sha_hex + (i * 2u), "%2x", &value) != 1) return false;
    digest[i] = (uint8_t)value;
  }
  if(out_size != nullptr) *out_size = (uint64_t)size_value;
  return true;
}

/**
 * Performs sd open record for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `path`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_open_record(const char *path) {
  if (s_file) {
    return ERR_SD_FAULT;
  }

  const error_code_t rc = sd_check_present();
  if (rc != ERR_NONE) {
    return rc;
  }

  File file = SD_MMC.open(path, FILE_APPEND);
  if (!file) {
    return sd_classify_io_fault();
  }

  s_file = file;
  strncpy(s_record_path, path, sizeof(s_record_path) - 1u);
  s_record_path[sizeof(s_record_path) - 1u] = '\0';
  mbedtls_sha256_init(&s_record_sha_ctx);
  s_record_sha_active = sd_sha256_starts_(&s_record_sha_ctx);
  s_record_sha_size = 0u;
  if(!s_record_sha_active) {
    s_file.close();
    s_file = File();
    s_record_path[0] = '\0';
    mbedtls_sha256_free(&s_record_sha_ctx);
    return ERR_SD_FAULT;
  }

  // Capture free space once at open time. The write path updates this cached
  // value locally so it does not need to query SD during recording.
  const error_code_t free_rc = sd_free_bytes(&s_cached_free_bytes);
  s_cached_free_valid = (free_rc == ERR_NONE);
  if (!s_cached_free_valid) {
    s_cached_free_bytes = 0u;
  }

  return ERR_NONE;
}

/**
 * Performs sd write record block for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `data`, `len`, `out_written`.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_write_record_block(const uint8_t *data, size_t len, size_t *out_written) {
  if (out_written) {
    *out_written = 0u;
  }

  if (!s_file) {
    return ERR_SD_FAULT;
  }

  const size_t written = s_file.write(data, len);
  if((written > 0u) && s_record_sha_active) {
    if(!sd_sha256_update_(&s_record_sha_ctx, data, written)) {
      s_record_sha_active = false;
    } else {
      s_record_sha_size += (uint64_t)written;
    }
  }
  if (out_written) {
    *out_written = written;
  }

  if (written == len) {
    if (s_cached_free_valid) {
      // Saturate at zero so the cache remains conservative after many writes.
      if (s_cached_free_bytes > written) {
        s_cached_free_bytes -= written;
      } else {
        s_cached_free_bytes = 0u;
      }

      if (s_cached_free_bytes < SD_RECORD_LOW_FREE_BYTES) {
        return ERR_SD_SPACE_LOW;
      }
    }
    return ERR_NONE;
  }

  // If the cached value is valid and already exhausted, report low-space rather
  // than a generic fault so the user sees the recoverable condition.
  if (s_cached_free_valid && s_cached_free_bytes == 0u) {
    return ERR_SD_SPACE_LOW;
  }

  return sd_classify_io_fault();
}

/**
 * Performs sd flush record for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_flush_record(void) {
  if (!s_file) {
    return ERR_SD_FAULT;
  }

  s_file.flush();
  return sd_check_present();
}

/**
 * Performs sd close record for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_close_record(void) {
  if (!s_file) {
    return ERR_SD_FAULT;
  }

  s_file.flush();
  s_file.close();
  s_file = File();

  uint8_t digest[32] = {};
  bool sha_ok = s_record_sha_active &&
                sd_sha256_finish_(&s_record_sha_ctx, digest);
  mbedtls_sha256_free(&s_record_sha_ctx);
  s_record_sha_active = false;

  if(sha_ok) {
    sha_ok = sd_sha_write_metadata_(s_record_path, s_record_sha_size, digest);
  }

  s_record_path[0] = '\0';
  s_record_sha_size = 0u;
  s_cached_free_bytes = 0u;
  s_cached_free_valid = false;
  return sha_ok ? ERR_NONE : ERR_SD_FAULT;
}


bool sd_storage_verify_root_recordings(sd_sha_verify_result_t *out_result,
                                       sd_sha_progress_cb_t progress_cb) {
  if(out_result == nullptr) return false;
  memset(out_result, 0, sizeof(*out_result));
  if(sd_check_present() != ERR_NONE) return false;

  File dir = SD_MMC.open("/");
  if(!dir || !dir.isDirectory()) {
    if(dir) dir.close();
    return false;
  }

  File entry;
  while((entry = dir.openNextFile())) {
    if(entry.isDirectory()) { entry.close(); continue; }
    const char *raw_name = entry.name();
    const char *entry_basename = sd_basename_local_(raw_name);
    char name[FILENAME_MAX_LENGTH] = {};
    const size_t copied = strlcpy(name, entry_basename, sizeof(name));
    const size_t name_len = strlen(name);
    entry.close();

    // File::name() storage belongs to the File object.  Keep a local copy
    // before closing the directory entry so SHA lookup and metadata filename
    // comparison use a stable name on every filesystem implementation.
    if((copied >= sizeof(name)) ||
       (name_len < 5u) ||
       (strcmp(name + name_len - 4u, ".bin") != 0)) {
      continue;
    }

    char bin_path[SD_STORAGE_PATH_MAX];
    char sha_path[SD_STORAGE_PATH_MAX];
    const int path_len = snprintf(bin_path, sizeof(bin_path), "/%s", name);
    if((path_len <= 0) || ((size_t)path_len >= sizeof(bin_path)) ||
       (!sd_sha_path_build_(bin_path, sha_path, sizeof(sha_path)))) continue;

    if(!SD_MMC.exists(sha_path)) {
      ++out_result->legacy_files;
      continue;
    }

    ++out_result->files_checked;
    uint64_t expected_size = 0u;
    uint8_t expected_digest[32] = {};
    uint8_t actual_digest[32] = {};
    uint64_t actual_size = 0u;

    bool valid = sd_sha_parse_metadata_(sha_path, name,
                                        &expected_size, expected_digest);
    if(!valid) {
      ++out_result->metadata_errors;
    } else {
      valid = sd_sha256_file_(bin_path, actual_digest, &actual_size, progress_cb);
      if(!valid || (actual_size != expected_size) ||
         (memcmp(actual_digest, expected_digest, sizeof(actual_digest)) != 0)) {
        ++out_result->sha_mismatches;
        valid = false;
      }
    }

    if(valid) {
      ++out_result->files_valid;
    } else if(out_result->first_error_file[0] == '\0') {
      strncpy(out_result->first_error_file, name,
              sizeof(out_result->first_error_file) - 1u);
    }
  }
  dir.close();
  return true;
}

/**
 * Begin an SD-owned sequential download session.  The file is opened once and
 * later consumed by sd_storage_download_read().
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the file was opened successfully.
 */
bool sd_storage_download_begin(const char *path, uint32_t *out_size) {
  if(sd_check_present() != ERR_NONE) return false;
  if((path == nullptr) || (out_size == nullptr)) return false;
  if(s_file) return false;  // recording file must not be open

  sd_storage_download_end();

  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if(!p) return false;

  s_download_file = SD_MMC.open(p, FILE_READ);
  if((!s_download_file) || s_download_file.isDirectory()){
    sd_storage_download_end();
    return false;
  }

  const size_t sz = s_download_file.size();
  if(sz > 0xFFFFFFFFu){
    sd_storage_download_end();
    return false;
  }

  s_download_size = (uint32_t)sz;
  s_download_offset = 0u;
  *out_size = s_download_size;
  return true;
}

/**
 * Read the next sequential chunk from the active SD-owned download session.
 *
 * Inputs: `out`, `len`, `out_len`.
 * Returns: `true` when the read operation completed.
 */
bool sd_storage_download_read(uint8_t *out, uint32_t len, uint32_t *out_len) {
  if((out == nullptr) || (out_len == nullptr)){
    return false;
  }

  *out_len = 0u;

  if(!s_download_file){
    return false;
  }

  if(s_download_offset >= s_download_size){
    return true;
  }

  const uint32_t remain = s_download_size - s_download_offset;
  const uint32_t to_read = (remain < len) ? remain : len;

  const size_t r = s_download_file.read(out, (size_t)to_read);
  *out_len = (uint32_t)r;
  s_download_offset += (uint32_t)r;

  return (r > 0u) || (to_read == 0u);
}

/**
 * End the active SD-owned download session.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_storage_download_end(void) {
  if(s_download_file){
    s_download_file.close();
  }
  s_download_file = File();
  s_download_size = 0u;
  s_download_offset = 0u;
}

/** Write a complete text file into an allowed SD text-output location. */
// Write a support text file through the SD owner. Accepted destinations are
// intentionally limited to calibration reports and root-level flight-analysis
// companion logs so Web support code cannot write arbitrary SD paths.
bool sd_storage_write_text_file(const char *path, const char *text, uint32_t len){
  if(sd_check_present() != ERR_NONE) return false;
  if((path == nullptr) || (text == nullptr)) return false;
  if(s_file || s_download_file) return false;

  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if(p == nullptr) return false;

  const bool calibration_report = (strncmp(p, "/calibration_reports/", 21) == 0);
  const bool flight_log = sd_path_is_root_file_(p) && sd_name_has_extension_(p, ".log");
  if((!calibration_report) && (!flight_log)){
    return false;
  }

  if(calibration_report && (!sd_ensure_calibration_reports_dir_())){
    return false;
  }

  File f = SD_MMC.open(p, FILE_WRITE);
  if(!f){
    return false;
  }

  const size_t wr = (len > 0u) ? f.write((const uint8_t*)text, (size_t)len) : 0u;
  f.flush();
  f.close();
  return wr == (size_t)len;
}

static bool sd_path_is_root_file_(const char *p){
  if((p == nullptr) || (p[0] != '/') || (p[1] == '\0')){
    return false;
  }

  // Only one slash is allowed: "/file.bin".  This preserves the current
  // root-file-only Web file-management model and keeps /processed hidden.
  return (strchr(p + 1, '/') == nullptr);
}

static bool sd_path_is_processed_file_(const char *p){
  static const char prefix[] = "/processed/";
  const size_t prefix_len = sizeof(prefix) - 1u;

  if((p == nullptr) || (strncmp(p, prefix, prefix_len) != 0)){
    return false;
  }

  const char *name = p + prefix_len;
  if(name[0] == '\0'){
    return false;
  }

  // Only a direct child of /processed is accepted.  This prevents directory
  // traversal and keeps this API limited to archived recorder files.
  return (strchr(name, '/') == nullptr) &&
         (strcmp(name, ".") != 0) &&
         (strcmp(name, "..") != 0);
}

static bool sd_path_is_calibration_report_file_(const char *p){
  static const char prefix[] = "/calibration_reports/";
  const size_t prefix_len = sizeof(prefix) - 1u;

  if((p == nullptr) || (strncmp(p, prefix, prefix_len) != 0)){
    return false;
  }

  const char *name = p + prefix_len;
  if(name[0] == '\0'){
    return false;
  }

  return (strchr(name, '/') == nullptr) &&
         (strcmp(name, ".") != 0) &&
         (strcmp(name, "..") != 0);
}

static const char *sd_basename_(const char *p){
  if(p == nullptr){
    return nullptr;
  }
  const char *slash = strrchr(p, '/');
  return (slash != nullptr) ? (slash + 1) : p;
}

static bool sd_entry_name_is_root_file_(const char *p){
  if((p == nullptr) || (p[0] == '\0')){
    return false;
  }

  // File.name() may return either "file.bin" or "/file.bin" for a
  // direct child of the SD root.  Anything containing another slash is not a
  // root-level file and must not be selected as today's active daily file.
  if(p[0] == '/'){
    ++p;
  }

  return (p[0] != '\0') && (strchr(p, '/') == nullptr);
}

static bool sd_split_name_ext_(const char *name,
                               char *base,
                               size_t base_sz,
                               char *ext,
                               size_t ext_sz){
  if((name == nullptr) || (base == nullptr) || (ext == nullptr) ||
     (base_sz == 0u) || (ext_sz == 0u)){
    return false;
  }

  const char *dot = strrchr(name, '.');
  if((dot == nullptr) || (dot == name)){
    dot = name + strlen(name);
  }

  const size_t base_len = (size_t)(dot - name);
  if((base_len == 0u) || (base_len >= base_sz)){
    return false;
  }

  memcpy(base, name, base_len);
  base[base_len] = '\0';

  if(*dot == '.'){
    const size_t ext_len = strlen(dot);
    if(ext_len >= ext_sz){
      return false;
    }
    memcpy(ext, dot, ext_len + 1u);
  }else{
    ext[0] = '\0';
  }

  return true;
}

static bool sd_path_exists_(const char *path){
  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if(p == nullptr){
    return true;
  }

  File f = SD_MMC.open(p);
  if(!f){
    return false;
  }
  f.close();
  return true;
}

static bool sd_build_unique_processed_path_(const char *base,
                                            const char *ext,
                                            char *dst,
                                            size_t dst_sz){
  if((base == nullptr) || (ext == nullptr) || (dst == nullptr) || (dst_sz == 0u)){
    return false;
  }

  const int n_initial = snprintf(dst, dst_sz, "/processed/%s%s", base, ext);
  if((n_initial < 0) || ((size_t)n_initial >= dst_sz)){
    return false;
  }

  if(!sd_path_exists_(dst)){
    return true;
  }

  for(uint32_t i = 1u; i <= 999u; ++i){
    const int n_suffix = snprintf(dst, dst_sz, "/processed/%s_%lu%s",
                                  base,
                                  (unsigned long)i,
                                  ext);
    if((n_suffix < 0) || ((size_t)n_suffix >= dst_sz)){
      return false;
    }

    if(!sd_path_exists_(dst)){
      return true;
    }
  }

  return false;
}

static bool sd_archive_companion_sha_(const char *src_bin, const char *dst_bin){
  if((src_bin == nullptr) || (dst_bin == nullptr) ||
     (!sd_name_has_extension_(src_bin, ".bin"))){
    return false;
  }

  char src_sha[SD_STORAGE_PATH_MAX];
  char dst_sha[SD_STORAGE_PATH_MAX];
  if((!sd_sha_path_build_(src_bin, src_sha, sizeof(src_sha))) ||
     (!sd_sha_path_build_(dst_bin, dst_sha, sizeof(dst_sha)))){
    return false;
  }

  // Legacy files have no creation SHA and remain fully supported.
  if(!sd_path_exists_(src_sha)){
    return true;
  }
  if(sd_path_exists_(dst_sha)){
    return false;
  }
  return SD_MMC.rename(src_sha, dst_sha);
}

static void sd_archive_companion_log_(const char *src_bin, const char *dst_bin){
  if((src_bin == nullptr) || (dst_bin == nullptr) || (!sd_name_has_extension_(src_bin, ".bin"))){
    return;
  }

  char src_log[SD_STORAGE_PATH_MAX];
  const size_t src_len = strlen(src_bin);
  if((src_len < 4u) || (src_len >= sizeof(src_log))){
    return;
  }
  memcpy(src_log, src_bin, src_len - 4u);
  memcpy(src_log + src_len - 4u, ".log", 5u);

  if(!sd_path_exists_(src_log)){
    return;
  }

  const char *dst_name = sd_basename_(dst_bin);
  char log_base[FILENAME_MAX_LENGTH];
  char log_ext[16];
  if(!sd_split_name_ext_(dst_name, log_base, sizeof(log_base), log_ext, sizeof(log_ext))){
    return;
  }

  char dst_log[SD_STORAGE_PATH_MAX];
  if(!sd_build_unique_processed_path_(log_base, ".log", dst_log, sizeof(dst_log))){
    return;
  }

  (void)SD_MMC.rename(src_log, dst_log);
}

static bool sd_ensure_processed_dir_(void){
  File d = SD_MMC.open("/processed");
  if(d){
    const bool is_dir = d.isDirectory();
    d.close();
    return is_dir;
  }

  return SD_MMC.mkdir("/processed");
}

static bool sd_ensure_calibration_reports_dir_(void){
  File d = SD_MMC.open("/calibration_reports");
  if(d){
    const bool is_dir = d.isDirectory();
    d.close();
    return is_dir;
  }

  return SD_MMC.mkdir("/calibration_reports");
}



/**
 * Parse the _N.bin suffix from a root file name matching one daily prefix.
 *
 * The expected daily filename is <prefix_base>_<N>.bin, for example
 * FCJAF_20260614_3.bin.  The prefix passed here does not include the leading
 * slash because File.name() can be returned with or without a path prefix.
 *
 * Inputs: `name`, `prefix_base`, `out_suffix`.
 * Returns: `true` when the name matches the daily pattern and N was parsed.
 */
static bool sd_daily_suffix_from_name_(const char *name,
                                       const char *prefix_base,
                                       uint32_t *out_suffix){
  if((name == nullptr) || (prefix_base == nullptr) || (out_suffix == nullptr)){
    return false;
  }

  const char *base = sd_basename_(name);
  if(base == nullptr){
    return false;
  }

  const size_t prefix_len = strlen(prefix_base);
  if((prefix_len == 0u) || (strncmp(base, prefix_base, prefix_len) != 0)){
    return false;
  }

  // Require the exact separator after the daily prefix.  This prevents
  // FCJAF_202606140_1.bin from matching FCJAF_20260614.
  const char *p = base + prefix_len;
  if(*p != '_'){
    return false;
  }
  ++p;

  if((*p < '1') || (*p > '9')){
    return false;
  }

  // New daily files use a small session counter: _1.bin, _2.bin, ...
  // Accepted daily-session suffixes are one to three decimal digits.  Six-digit
  // time suffixes are outside the daily-file naming scheme and are ignored.
  static const uint32_t DAILY_SESSION_SUFFIX_MAX = 999u;
  uint32_t value = 0u;
  uint32_t digit_count = 0u;
  while((*p >= '0') && (*p <= '9')){
    ++digit_count;
    if(digit_count > 3u){
      return false;
    }

    const uint32_t digit = (uint32_t)(*p - '0');
    value = (value * 10u) + digit;
    if(value > DAILY_SESSION_SUFFIX_MAX){
      return false;
    }
    ++p;
  }

  if((value == 0u) || (strcmp(p, ".bin") != 0)){
    return false;
  }

  *out_suffix = value;
  return true;
}

/**
 * Build a full daily recording path from prefix and session suffix.
 *
 * Inputs: `prefix`, `session_index`, `out`, `out_sz`.
 * Returns: `true` when the path fits in the output buffer.
 */
static bool sd_daily_path_build_(const char *prefix,
                                 uint32_t session_index,
                                 char *out,
                                 size_t out_sz){
  if((prefix == nullptr) || (out == nullptr) || (out_sz == 0u) ||
     (session_index == 0u)){
    return false;
  }

  const int n = snprintf(out, out_sz, "%s_%lu.bin",
                         prefix,
                         (unsigned long)session_index);
  return (n > 0) && ((size_t)n < out_sz);
}

/**
 * Scan one directory and update the highest immutable daily-session suffix.
 *
 * Only files matching <prefix_base>_<N>.bin are considered. Other files,
 * directories, logs, and future SHA metadata are ignored. A missing optional
 * directory such as /processed is treated as empty.
 *
 * Inputs: `directory`, `prefix_base`, `directory_required`, `inout_highest`.
 * Returns: `ERR_NONE` on success; otherwise an SD error code.
 */
static error_code_t sd_daily_highest_suffix_in_dir_(const char *directory,
                                                    const char *prefix_base,
                                                    bool directory_required,
                                                    uint32_t *inout_highest){
  if((directory == nullptr) || (prefix_base == nullptr) ||
     (inout_highest == nullptr)){
    return ERR_SD_FAULT;
  }

  File dir = SD_MMC.open(directory);
  if(!dir){
    return directory_required ? sd_classify_io_fault() : ERR_NONE;
  }

  if(!dir.isDirectory()){
    dir.close();
    return ERR_SD_FAULT;
  }

  for(;;){
    File entry = dir.openNextFile();
    if(!entry){
      break;
    }

    if(!entry.isDirectory()){
      uint32_t suffix = 0u;
      if(sd_daily_suffix_from_name_(entry.name(), prefix_base, &suffix) &&
         (suffix > *inout_highest)){
        *inout_highest = suffix;
      }
    }

    entry.close();
  }

  dir.close();
  return ERR_NONE;
}

/**
 * Open a new immutable recording-session file for one registration/date.
 *
 * The highest existing suffix is found across the SD root and /processed.
 * The new session is created as <prefix>_<highest+1>.bin. Existing files are
 * never renamed, restored, reopened, or appended. This preserves every closed
 * recording file as an immutable source artifact.
 *
 * Legacy daily files created by earlier firmware remain valid: their suffixes
 * participate in allocation, but their contents are never modified.
 *
 * Inputs: `prefix` in the form /REGISTRATION_YYYYMMDD.
 * Returns: `ERR_NONE` on success; otherwise an SD error code.
 */
error_code_t sd_open_record_daily(const char *prefix){
  if(s_file){
    return ERR_SD_FAULT;
  }

  const error_code_t rc = sd_check_present();
  if(rc != ERR_NONE){
    return rc;
  }

  if((prefix == nullptr) || (prefix[0] != '/') || (prefix[1] == '\0') ||
     (strchr(prefix + 1, '/') != nullptr)){
    return ERR_SD_FAULT;
  }

  const char *prefix_base = prefix + 1;
  uint32_t highest_suffix = 0u;

  error_code_t scan_rc = sd_daily_highest_suffix_in_dir_("/",
                                                         prefix_base,
                                                         true,
                                                         &highest_suffix);
  if(scan_rc != ERR_NONE){
    return scan_rc;
  }

  scan_rc = sd_daily_highest_suffix_in_dir_("/processed",
                                            prefix_base,
                                            false,
                                            &highest_suffix);
  if(scan_rc != ERR_NONE){
    return scan_rc;
  }

  static const uint32_t DAILY_SESSION_SUFFIX_MAX = 999u;
  if(highest_suffix >= DAILY_SESSION_SUFFIX_MAX){
    return ERR_SD_FAULT;
  }

  const uint32_t next_suffix = highest_suffix + 1u;
  char target_path[SD_STORAGE_PATH_MAX];
  if(!sd_daily_path_build_(prefix, next_suffix,
                           target_path, sizeof(target_path))){
    return ERR_SD_FAULT;
  }

  // The suffix scan covers both active and archived files. Refuse any collision
  // instead of modifying an existing immutable file.
  if(sd_path_exists_(target_path)){
    return ERR_SD_FAULT;
  }

  return sd_open_record(target_path);
}


/**
 * Archive a root-level recording file or calibration report by moving it into
 * /processed.  Name collisions are resolved by appending _N before the
 * extension.
 *
 * Inputs: `path`.
 * Returns: `true` when the file was moved; otherwise `false`.
 */
bool sd_storage_archive_to_processed(const char *path) {
  if(sd_check_present() != ERR_NONE) return false;
  if(path == nullptr) return false;

  char src_tmp[SD_STORAGE_PATH_MAX];
  const char *src = sd_norm_sdmmc_path_(path, src_tmp, sizeof(src_tmp));
  if(src == nullptr) return false;

  if((!sd_path_is_root_file_(src)) &&
     (!sd_path_is_calibration_report_file_(src))){
    return false;
  }

  File src_file = SD_MMC.open(src, FILE_READ);
  if(!src_file){
    return false;
  }

  if(src_file.isDirectory()){
    src_file.close();
    return false;
  }
  src_file.close();

  if(!sd_ensure_processed_dir_()){
    return false;
  }

  const char *name = sd_basename_(src);
  char base[FILENAME_MAX_LENGTH];
  char ext[16];
  if(!sd_split_name_ext_(name, base, sizeof(base), ext, sizeof(ext))){
    return false;
  }

  char dst[SD_STORAGE_PATH_MAX];
  if(!sd_build_unique_processed_path_(base, ext, dst, sizeof(dst))){
    return false;
  }

  const bool moved = SD_MMC.rename(src, dst);
  if(!moved){
    return false;
  }

  if(sd_path_is_root_file_(src)){
    // A new immutable recording and its creation SHA form one archive unit.
    // Roll the binary move back if companion SHA archival fails. Legacy files
    // without SHA metadata continue normally.
    if(!sd_archive_companion_sha_(src, dst)){
      (void)SD_MMC.rename(dst, src);
      return false;
    }
    sd_archive_companion_log_(src, dst);
  }

  return true;
}

/**
 * Performs sd storage list json for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: `dir_path`, `out_json`, `out_cap`, `out_len`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_list_json(const char *dir_path, char *out_json, uint32_t out_cap, uint32_t *out_len) {
  if((out_json == nullptr) || (out_cap < 3u)){
    if(out_len != nullptr) *out_len = 0u;
    return false;
  }

  out_json[0] = '[';
  out_json[1] = ']';
  out_json[2] = '\0';
  if(out_len != nullptr) *out_len = 2u;

  if(sd_check_present() != ERR_NONE){
    return false;
  }

  char dir_tmp[SD_STORAGE_PATH_MAX];
  const char *dir = sd_norm_sdmmc_path_(dir_path, dir_tmp, sizeof(dir_tmp));
  if(dir == nullptr){
    return false;
  }

  // Only actionable root recordings, calibration reports, and the virtual
  // logbook view are exposed. /processed remains hidden from normal Web
  // listings. The /install_restore/<registration> virtual view is support-only
  // and returns matching installation-calibration report names found in both
  // /calibration_reports and /processed.
  const bool list_root = (strcmp(dir, "/") == 0);
  const bool list_reports = (strcmp(dir, "/calibration_reports") == 0);
  const bool list_logbook = (strcmp(dir, "/logbook") == 0);
  static const char install_restore_prefix[] = "/install_restore/";
  const size_t install_restore_prefix_len = sizeof(install_restore_prefix) - 1u;
  const bool list_install_restore =
      (strncmp(dir, install_restore_prefix, install_restore_prefix_len) == 0) &&
      (dir[install_restore_prefix_len] != '\0') &&
      (strchr(dir + install_restore_prefix_len, '/') == nullptr);
  const char *install_restore_registration =
      list_install_restore ? (dir + install_restore_prefix_len) : nullptr;
  if((!list_root) && (!list_reports) && (!list_logbook) && (!list_install_restore)){
    return false;
  }

  uint32_t count = 0u;
  sd_storage_list_item_t list_items[SD_MAX_RECORD_FILES];

  auto add_item = [&](const char *name, uint32_t size, bool retain_newest_only) {
    if((name == nullptr) || (name[0] == '\0')) return;

    // A log can temporarily exist in root and later in /processed. Keep only
    // one entry for an identical file name.
    for(uint32_t i = 0u; i < count; i++){
      if(strcmp(list_items[i].name, name) == 0){
        list_items[i].size = size;
        return;
      }
    }

    uint32_t slot = count;
    if(count >= (uint32_t)SD_MAX_RECORD_FILES){
      if(!retain_newest_only) return;

      // The archive may contain thousands of logs. Retain only the newest
      // SD_MAX_RECORD_FILES names while scanning, using constant memory.
      uint32_t oldest = 0u;
      for(uint32_t i = 1u; i < count; i++){
        if(strcmp(list_items[i].name, list_items[oldest].name) < 0){
          oldest = i;
        }
      }
      if(strcmp(name, list_items[oldest].name) <= 0) return;
      slot = oldest;
    } else {
      count++;
    }

    strlcpy(list_items[slot].name, name, sizeof(list_items[slot].name));
    list_items[slot].size = size;
  };

  auto scan_dir = [&](const char *scan_path, bool logs_only, bool optional_dir) -> bool {
    File root = SD_MMC.open(scan_path);
    if(!root){
      return optional_dir;
    }
    if(!root.isDirectory()){
      root.close();
      return false;
    }

    for(;;){
      File file = root.openNextFile();
      if(!file) break;
      if(file.isDirectory()){
        file.close();
        continue;
      }

      const char *raw_name = file.name();
      const char *base_name = sd_basename_(raw_name);
      char name[FILENAME_MAX_LENGTH] = {};
      if(base_name != nullptr){
        strlcpy(name, base_name, sizeof(name));
      }
      const uint32_t size = (uint32_t)file.size();
      file.close();

      if(name[0] == '\0') continue;
      if(logs_only){
        if(!sd_name_has_extension_(name, ".log")) continue;
        add_item(name, size, true);
      } else if(list_install_restore){
        const size_t reg_len = strlen(install_restore_registration);
        if(strncmp(name, install_restore_registration, reg_len) != 0) continue;
        if(name[reg_len] != '_') continue;
        if(strstr(name, "_INST_CAL") == nullptr) continue;
        if(!sd_name_has_extension_(name, ".txt")) continue;
        add_item(name, size, true);
      } else if(list_root){
        if(!sd_name_has_extension_(name, ".bin")) continue;
        add_item(name, size, false);
      } else {
        add_item(name, size, false);
      }
    }
    root.close();
    return true;
  };

  if(list_logbook){
    // A newly analysed log can still be in root, while older logs are already
    // archived. Scan both locations and deduplicate by file name.
    if(!scan_dir("/", true, false)) return false;
    if(!scan_dir("/processed", true, true)) return false;
  } else if(list_install_restore){
    // Installation calibration reports may still be pending upload or may have
    // already been archived after upload. Search both without exposing the
    // /processed directory through the normal file-management API.
    if(!scan_dir("/calibration_reports", false, true)) return false;
    if(!scan_dir("/processed", false, true)) return false;
  } else {
    if(!scan_dir(dir, false, list_reports)) return false;
  }

  if(count > 1u){
    auto cmp = [](const void* a, const void* b) -> int {
      const sd_storage_list_item_t* ia = (const sd_storage_list_item_t*)a;
      const sd_storage_list_item_t* ib = (const sd_storage_list_item_t*)b;
      return strcmp(ia->name, ib->name);
    };
    qsort(list_items, (size_t)count, sizeof(list_items[0]), cmp);
  }

  uint32_t used = 2u;
  bool first = true;

  for(uint32_t i = 0u; i < count; i++){
    char entry[SD_FILE_LIST_JSON_ENTRY_MAX];
    const int n_written = snprintf(entry, sizeof(entry),
                                   "{\"name\":\"%s\",\"size\":%lu,\"isDir\":false}",
                                   list_items[i].name, (unsigned long)list_items[i].size);
    if((n_written <= 0) || ((size_t)n_written >= sizeof(entry))) break;

    const uint32_t need = (uint32_t)n_written + (first ? 0u : 1u);
    if((used + need) >= out_cap) break;

    uint32_t pos = used - 1u;
    if(!first){
      out_json[pos++] = ',';
    }
    memcpy(&out_json[pos], entry, (size_t)n_written);
    pos += (uint32_t)n_written;
    out_json[pos++] = ']';
    out_json[pos] = '\0';
    used = pos;
    first = false;
  }

  if(out_len != nullptr) *out_len = used;
  return true;
}
