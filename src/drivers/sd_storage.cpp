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

// Threshold below which SD space is considered too low for continued recording.
static constexpr uint64_t SD_SPACE_LOW_BYTES = (uint64_t)SD_SPACE_LOW_MB * 1024ULL * 1024ULL;

// True after a successful mount / detect sequence.
static bool s_mounted = false;

// Cached low-level card handle needed for sdmmc_get_status().
static sdmmc_card_t *s_card = nullptr;

// Current open recording file, if any.
static File s_file;

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
error_code_t sd_begin(void) {
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
  return sd_begin();
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
    if (!entry.isDirectory()) {
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

  if (free_bytes < SD_SPACE_LOW_BYTES) {
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
 * Performs sd storage read for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `path`, `offset`, `len`, `out`, `out_len`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_read(const char *path, uint32_t offset, uint32_t len, uint8_t *out, uint32_t *out_len) {
  if(sd_check_present() != ERR_NONE) return false;
  if(!path || !out || !out_len) return false;

  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if(!p) return false;

  File f = SD_MMC.open(p, FILE_READ);
  if(!f) return false;

  if(offset > 0u){
    if(!f.seek(offset)){
      f.close();
      return false;
    }
  }

  const size_t r = f.read(out, len);
  f.close();
  *out_len = (uint32_t)r;
  return true;
}


static bool sd_path_is_root_file_(const char *p){
  if((p == nullptr) || (p[0] != '/') || (p[1] == '\0')){
    return false;
  }

  // Only one slash is allowed: "/file.bin".  This preserves the current
  // root-file-only Web file-management model and keeps /processed hidden.
  return (strchr(p + 1, '/') == nullptr);
}

static const char *sd_basename_(const char *p){
  if(p == nullptr){
    return nullptr;
  }
  const char *slash = strrchr(p, '/');
  return (slash != nullptr) ? (slash + 1) : p;
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

static bool sd_ensure_processed_dir_(void){
  File d = SD_MMC.open("/processed");
  if(d){
    const bool is_dir = d.isDirectory();
    d.close();
    return is_dir;
  }

  return SD_MMC.mkdir("/processed");
}


/**
 * Performs sd storage delete for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `path`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_delete(const char *path) {
  if(sd_check_present() != ERR_NONE) return false;
  if(!path) return false;
  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if(!p) return false;
  return SD_MMC.remove(p);
}

/**
 * Archive a root-level file by moving it into /processed.  Name collisions are
 * resolved by appending _N before the extension.
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

  if(!sd_path_is_root_file_(src)){
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
  const int n_initial = snprintf(dst, sizeof(dst), "/processed/%s%s", base, ext);
  if((n_initial < 0) || ((size_t)n_initial >= sizeof(dst))){
    return false;
  }

  if(!sd_path_exists_(dst)){
    return SD_MMC.rename(src, dst);
  }

  for(uint32_t i = 1u; i <= 999u; ++i){
    const int n_suffix = snprintf(dst, sizeof(dst), "/processed/%s_%lu%s",
                                  base,
                                  (unsigned long)i,
                                  ext);
    if((n_suffix < 0) || ((size_t)n_suffix >= sizeof(dst))){
      return false;
    }

    if(!sd_path_exists_(dst)){
      return SD_MMC.rename(src, dst);
    }
  }

  return false;
}

/**
 * Performs sd storage move for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: `src`, `dst`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_move(const char *src, const char *dst) {
  if(sd_check_present() != ERR_NONE) return false;
  if(!src || !dst) return false;
  char ts[SD_STORAGE_PATH_MAX];
  char td[SD_STORAGE_PATH_MAX];
  const char *ps = sd_norm_sdmmc_path_(src, ts, sizeof(ts));
  const char *pd = sd_norm_sdmmc_path_(dst, td, sizeof(td));
  if(!ps || !pd) return false;
  return SD_MMC.rename(ps, pd);
}

/**
 * Returns the requested sd storage get file size information from the module
 * state or underlying driver interface.
 *
 * Inputs: `path`, `out_size`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_storage_get_file_size(const char *path, uint32_t *out_size) {
  if(out_size != nullptr) *out_size = 0u;
  if(sd_check_present() != ERR_NONE) return false;
  if(!path || !out_size) return false;

  char tmp[SD_STORAGE_PATH_MAX];
  const char *p = sd_norm_sdmmc_path_(path, tmp, sizeof(tmp));
  if(!p) return false;

  File f = SD_MMC.open(p);
  if(!f){
    return false;
  }

  if(f.isDirectory()){
    f.close();
    return false;
  }

  *out_size = (uint32_t)f.size();
  f.close();
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
  (void)dir_path;
  if((out_json == nullptr) || (out_cap < 3u)){
    if(out_len != nullptr) *out_len = 0u;
    return false;
  }
  if(sd_check_present() != ERR_NONE){
    if(out_len != nullptr) *out_len = 0u;
    out_json[0] = '['; out_json[1] = ']'; out_json[2] = '\0';
    return false;
  }

  uint32_t count = 0u;
  sd_storage_list_item_t list_items[SD_MAX_RECORD_FILES];

  File root = SD_MMC.open("/");
  if(!root){
    if(out_len != nullptr) *out_len = 0u;
    out_json[0] = '['; out_json[1] = ']'; out_json[2] = '\0';
    return false;
  }

  for(;;){
    File file = root.openNextFile();
    if(!file) break;
    if(count >= (uint32_t)SD_MAX_RECORD_FILES){ file.close(); break; }
    if(file.isDirectory()){ file.close(); continue; }

    const char *name = file.name();
    const uint32_t size = (uint32_t)file.size();
    file.close();

    if(name == nullptr){
      list_items[count].name[0] = '\0';
    } else {
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

  out_json[0] = '['; out_json[1] = ']'; out_json[2] = '\0';
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
