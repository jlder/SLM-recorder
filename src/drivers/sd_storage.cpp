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

#include "sdmmc_cmd.h"
#include "src/board/pin_config.h"
#include "config.h"
#include <cstring>
#include <cstdlib>

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

  s_file.close();
  s_file = File();
  s_cached_free_bytes = 0u;
  s_cached_free_valid = false;
  return ERR_NONE;
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

  if(!sd_entry_name_is_root_file_(name)){
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
 * Open the daily recording file and append a new recording session.
 *
 * Normal case: zero or one file exists for a given registration/date prefix.
 * If today's file exists, it is renamed from _N.bin to _(N+1).bin before the
 * new session is appended.  If more than one matching file exists, the function
 * reports a fault instead of guessing which file should receive the new data.
 *
 * Inputs: `prefix`.
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
  uint32_t match_count = 0u;
  uint32_t current_session = 0u;
  char current_path[SD_STORAGE_PATH_MAX] = "";

  File root = SD_MMC.open("/");
  if(!root){
    return sd_classify_io_fault();
  }

  for(;;){
    File entry = root.openNextFile();
    if(!entry){
      break;
    }

    const char *entry_name = entry.name();
    uint32_t suffix = 0u;
    const bool matches = sd_daily_suffix_from_name_(entry_name,
                                                    prefix_base,
                                                    &suffix);

    if(matches){
      if(entry.isDirectory()){
        entry.close();
        root.close();
        return ERR_SD_FAULT;
      }

      ++match_count;
      if(match_count > 1u){
        entry.close();
        root.close();
        return ERR_SD_FAULT;
      }

      current_session = suffix;

      // Store the root path to the only matching daily file.  This path is
      // used for the rename before opening in append mode.
      const char *base = sd_basename_(entry_name);
      if(base == nullptr){
        entry.close();
        root.close();
        return ERR_SD_FAULT;
      }

      const int n = snprintf(current_path, sizeof(current_path), "/%s", base);
      if((n <= 0) || ((size_t)n >= sizeof(current_path))){
        entry.close();
        root.close();
        return ERR_SD_FAULT;
      }
    }

    entry.close();
  }

  root.close();

  const uint32_t next_session = (match_count == 0u) ? 1u : (current_session + 1u);
  if((next_session == 0u) || ((match_count != 0u) && (next_session <= current_session))){
    return ERR_SD_FAULT;
  }

  char target_path[SD_STORAGE_PATH_MAX];
  if(!sd_daily_path_build_(prefix, next_session, target_path, sizeof(target_path))){
    return ERR_SD_FAULT;
  }

  if(match_count == 1u){
    if(sd_path_exists_(target_path)){
      return ERR_SD_FAULT;
    }

    if(!SD_MMC.rename(current_path, target_path)){
      return sd_classify_io_fault();
    }
  }

  const error_code_t open_rc = sd_open_record(target_path);
  if(open_rc != ERR_NONE){
    return open_rc;
  }

  return ERR_NONE;
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
  if(moved && sd_path_is_root_file_(src)){
    sd_archive_companion_log_(src, dst);
  }

  return moved;
}

/**
 * Permanently delete one archived file from /processed.
 *
 * Inputs: `path`.
 * Returns: `true` when the selected archived file was deleted.
 */
bool sd_storage_delete_processed_file(const char *path) {
  if(sd_check_present() != ERR_NONE) return false;
  if(path == nullptr) return false;

  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if((p == nullptr) || (!sd_path_is_processed_file_(p))){
    return false;
  }

  File f = SD_MMC.open(p, FILE_READ);
  if(!f){
    return false;
  }

  if(f.isDirectory()){
    f.close();
    return false;
  }
  f.close();

  return SD_MMC.remove(p);
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

  // Web file management lists the root recording area, /processed, or the
  // calibration report folder. Other directories are intentionally not exposed
  // through the Web API.
  const bool list_root = (strcmp(dir, "/") == 0);
  const bool list_processed = (strcmp(dir, "/processed") == 0);
  const bool list_reports = (strcmp(dir, "/calibration_reports") == 0);
  // /logbook is a virtual Web-only view over .log files in /processed. These
  // are the companion logs of recordings already downloaded and archived.
  const bool list_logbook = (strcmp(dir, "/logbook") == 0);
  if((!list_root) && (!list_processed) && (!list_reports) && (!list_logbook)){
    return false;
  }

  File root = SD_MMC.open(list_logbook ? "/processed" : dir);
  if(!root){
    // /processed and /calibration_reports are created on first use. Before
    // that, the corresponding maintenance and logbook pages are simply empty.
    return list_processed || list_reports || list_logbook;
  }

  if(!root.isDirectory()){
    root.close();
    return false;
  }

  uint32_t count = 0u;
  sd_storage_list_item_t list_items[SD_MAX_RECORD_FILES];

  for(;;){
    File file = root.openNextFile();
    if(!file) break;
    if(count >= (uint32_t)SD_MAX_RECORD_FILES){ file.close(); break; }
    if(file.isDirectory()){ file.close(); continue; }

    const char *raw_name = file.name();
    const char *name = sd_basename_(raw_name);
    const uint32_t size = (uint32_t)file.size();
    file.close();

    if((name == nullptr) || (name[0] == '\0')){
      continue;
    }

    if(list_root && (!sd_name_has_extension_(name, ".bin"))){
      continue;
    }
    if(list_logbook && (!sd_name_has_extension_(name, ".log"))){
      continue;
    }

    {
      size_t n = 0u;
      while((n < (sizeof(list_items[count].name) - 1u)) && (name[n] != '\0')){
        list_items[count].name[n] = name[n];
        n++;
      }
      list_items[count].name[n] = '\0';
    }
    list_items[count].size = size;
    count++;
  }
  root.close();

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
