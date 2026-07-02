// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_store.cpp
 * @brief Persistent storage for recorder and installation calibration.
 */

#include "src/services/calibration_store.h"

#include <Preferences.h>
#include <string.h>
#include "config.h"

static Preferences s_cal_prefs;
static bool s_cal_storage_ready = false;

// Current split-storage keys.  Sensor and installation calibration are stored
// independently so a future change to one format does not invalidate the other.
//
// Storage-maintenance rule:
// - These keys and the packed record structures below define the persistent NVS
//   calibration storage schema.
// - Any incompatible change to a packed record layout, field meaning, checksum
//   coverage, or key meaning must be accompanied by a matching bump of
//   CALIBRATION_SENSOR_STORAGE_VERSION or CALIBRATION_INSTALL_STORAGE_VERSION
//   in config.h.
// - The load path shall reject incompatible old versions unless an explicit
//   migration is implemented and documented. v1.14 changes recorder calibration
//   storage and acceptance semantics; installation calibration storage remains
//   version 1 because its payload format is unchanged.
static const char *KEY_SENSOR = "sensor";
static const char *KEY_SENSOR_REF = "sensor_ref";
static const char *KEY_SENSOR_CAND = "sensor_cand";
static const char *KEY_INSTALL = "install";
static const char *KEY_FAULT = "fault";
static const char *KEY_FAULT_REASON = "fault_reason";

// The following packed types are the on-flash NVS payload format. They are
// intentionally separate from runtime calibration structs to avoid compiler
// padding/bool-layout dependencies. If any packed type changes incompatibly,
// bump the affected CALIBRATION_*_STORAGE_VERSION in config.h.
typedef struct __attribute__((packed)) {
  float x_mg;
  float y_mg;
  float z_mg;
} calibration_store_vec_t;

typedef struct __attribute__((packed)) {
  uint8_t valid;
  calibration_store_vec_t mean_mg;
  calibration_store_vec_t stddev_mg;
} calibration_store_face_t;

typedef struct __attribute__((packed)) {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
} calibration_store_datetime_t;

typedef struct __attribute__((packed)) {
  uint8_t valid;
  calibration_store_datetime_t timestamp;
  float gravity_mg;
  float gain_x;
  float gain_y;
  float gain_z;
  float offset_x_mg;
  float offset_y_mg;
  float offset_z_mg;
  uint8_t temperature_valid;
  float temperature_c;
  float temperature_min_c;
  float temperature_max_c;
  calibration_store_face_t face[CAL_FACE_COUNT];
} calibration_store_sensor_t;

typedef struct __attribute__((packed)) {
  uint8_t valid;
  calibration_store_datetime_t timestamp;
  calibration_store_vec_t mean_mg;
  calibration_store_vec_t stddev_mg;
  float matrix[9];
} calibration_store_installation_t;

typedef struct __attribute__((packed)) {
  uint32_t version;
  calibration_store_sensor_t sensor;
  uint32_t checksum;
} calibration_store_sensor_record_t;

typedef struct __attribute__((packed)) {
  uint32_t version;
  calibration_store_installation_t installation;
  uint32_t checksum;
} calibration_store_installation_record_t;

static void datetime_to_store_(calibration_store_datetime_t *out, const rtc_datetime_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }
  out->year = in->year;
  out->month = in->month;
  out->day = in->day;
  out->hour = in->hour;
  out->min = in->min;
  out->sec = in->sec;
}

static void datetime_from_store_(rtc_datetime_t *out, const calibration_store_datetime_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }
  out->year = in->year;
  out->month = in->month;
  out->day = in->day;
  out->hour = in->hour;
  out->min = in->min;
  out->sec = in->sec;
}

static void vec_to_store_(calibration_store_vec_t *out, const calibration_vec_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }
  out->x_mg = in->x_mg;
  out->y_mg = in->y_mg;
  out->z_mg = in->z_mg;
}

static void vec_from_store_(calibration_vec_t *out, const calibration_store_vec_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }
  out->x_mg = in->x_mg;
  out->y_mg = in->y_mg;
  out->z_mg = in->z_mg;
}

static void sensor_to_store_(calibration_store_sensor_t *out, const sensor_calibration_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->valid = in->valid ? 1u : 0u;
  datetime_to_store_(&out->timestamp, &in->timestamp);
  out->gravity_mg = in->gravity_mg;
  out->gain_x = in->gain_x;
  out->gain_y = in->gain_y;
  out->gain_z = in->gain_z;
  out->offset_x_mg = in->offset_x_mg;
  out->offset_y_mg = in->offset_y_mg;
  out->offset_z_mg = in->offset_z_mg;
  out->temperature_valid = in->temperature_valid ? 1u : 0u;
  out->temperature_c = in->temperature_c;
  out->temperature_min_c = in->temperature_min_c;
  out->temperature_max_c = in->temperature_max_c;
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    out->face[i].valid = in->face[i].valid ? 1u : 0u;
    vec_to_store_(&out->face[i].mean_mg, &in->face[i].mean_mg);
    vec_to_store_(&out->face[i].stddev_mg, &in->face[i].stddev_mg);
  }
}

static void sensor_from_store_(sensor_calibration_t *out, const calibration_store_sensor_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->valid = (in->valid != 0u);
  datetime_from_store_(&out->timestamp, &in->timestamp);
  out->gravity_mg = in->gravity_mg;
  out->gain_x = in->gain_x;
  out->gain_y = in->gain_y;
  out->gain_z = in->gain_z;
  out->offset_x_mg = in->offset_x_mg;
  out->offset_y_mg = in->offset_y_mg;
  out->offset_z_mg = in->offset_z_mg;
  out->temperature_valid = (in->temperature_valid != 0u);
  out->temperature_c = in->temperature_c;
  out->temperature_min_c = in->temperature_min_c;
  out->temperature_max_c = in->temperature_max_c;
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    out->face[i].valid = (in->face[i].valid != 0u);
    vec_from_store_(&out->face[i].mean_mg, &in->face[i].mean_mg);
    vec_from_store_(&out->face[i].stddev_mg, &in->face[i].stddev_mg);
  }
}

static void installation_to_store_(calibration_store_installation_t *out,
                                   const installation_calibration_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->valid = in->valid ? 1u : 0u;
  datetime_to_store_(&out->timestamp, &in->timestamp);
  vec_to_store_(&out->mean_mg, &in->mean_mg);
  vec_to_store_(&out->stddev_mg, &in->stddev_mg);
  memcpy(out->matrix, in->matrix, sizeof(out->matrix));
}

static void installation_from_store_(installation_calibration_t *out,
                                     const calibration_store_installation_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->valid = (in->valid != 0u);
  datetime_from_store_(&out->timestamp, &in->timestamp);
  vec_from_store_(&out->mean_mg, &in->mean_mg);
  vec_from_store_(&out->stddev_mg, &in->stddev_mg);
  memcpy(out->matrix, in->matrix, sizeof(out->matrix));
}

/**
 * Compute a simple additive checksum over an explicit packed persistent byte
 * format, excluding the checksum field. This avoids depending on runtime C++
 * structure padding or bool representation.
 */
static uint32_t checksum_bytes_(const void *data, size_t len){
  if(data == nullptr){
    return 0u;
  }

  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t sum = 0u;
  for(size_t i = 0u; i < len; ++i){
    sum += (uint32_t)bytes[i];
  }
  return sum;
}

static uint32_t sensor_record_checksum_(const calibration_store_sensor_record_t *rec){
  if(rec == nullptr){
    return 0u;
  }

  calibration_store_sensor_record_t tmp = *rec;
  tmp.checksum = 0u;
  return checksum_bytes_(&tmp, sizeof(tmp));
}

static uint32_t installation_record_checksum_(const calibration_store_installation_record_t *rec){
  if(rec == nullptr){
    return 0u;
  }

  calibration_store_installation_record_t tmp = *rec;
  tmp.checksum = 0u;
  return checksum_bytes_(&tmp, sizeof(tmp));
}

static bool sensor_record_valid_(const calibration_store_sensor_record_t *rec){
  if(rec == nullptr){
    return false;
  }
  if(rec->version != (uint32_t)CALIBRATION_SENSOR_STORAGE_VERSION){
    return false;
  }
  if(rec->sensor.valid == 0u){
    return false;
  }
  return (rec->checksum == sensor_record_checksum_(rec));
}

static bool installation_record_valid_(const calibration_store_installation_record_t *rec){
  if(rec == nullptr){
    return false;
  }
  if(rec->version != (uint32_t)CALIBRATION_INSTALL_STORAGE_VERSION){
    return false;
  }
  if(rec->installation.valid == 0u){
    return false;
  }
  return (rec->checksum == installation_record_checksum_(rec));
}

static bool calibration_store_save_sensor_key_(const char *key, const sensor_calibration_t *sensor){
  if((key == nullptr) || (sensor == nullptr) || !sensor->valid || !s_cal_storage_ready){
    return false;
  }

  calibration_store_sensor_record_t stored = {};
  stored.version = (uint32_t)CALIBRATION_SENSOR_STORAGE_VERSION;
  sensor_to_store_(&stored.sensor, sensor);
  stored.sensor.valid = 1u;
  stored.checksum = sensor_record_checksum_(&stored);

  const size_t n = s_cal_prefs.putBytes(key, &stored, sizeof(stored));
  return (n == sizeof(stored));
}

static bool calibration_store_save_sensor_(const sensor_calibration_t *sensor){
  return calibration_store_save_sensor_key_(KEY_SENSOR, sensor);
}

static bool calibration_store_save_installation_(const installation_calibration_t *installation){
  if((installation == nullptr) || !installation->valid || !s_cal_storage_ready){
    return false;
  }

  calibration_store_installation_record_t stored = {};
  stored.version = (uint32_t)CALIBRATION_INSTALL_STORAGE_VERSION;
  installation_to_store_(&stored.installation, installation);
  stored.installation.valid = 1u;
  stored.checksum = installation_record_checksum_(&stored);

  const size_t n = s_cal_prefs.putBytes(KEY_INSTALL, &stored, sizeof(stored));
  return (n == sizeof(stored));
}


static bool calibration_store_load_sensor_key_(const char *key, sensor_calibration_t *out){
  if((key == nullptr) || (out == nullptr) || !s_cal_storage_ready || !s_cal_prefs.isKey(key)){
    return false;
  }

  const size_t len = s_cal_prefs.getBytesLength(key);
  if(len == sizeof(calibration_store_sensor_record_t)){
    calibration_store_sensor_record_t stored = {};
    const size_t n = s_cal_prefs.getBytes(key, &stored, sizeof(stored));
    if((n == sizeof(stored)) && sensor_record_valid_(&stored)){
      sensor_from_store_(out, &stored.sensor);
      return true;
    }
  }

  // Legacy v1 records did not include mandatory temperature/history
  // semantics. Reject them so v1.14 forces fresh recorder calibration.
  return false;
}

static bool calibration_store_load_sensor_(sensor_calibration_t *out){
  return calibration_store_load_sensor_key_(KEY_SENSOR, out);
}

static bool calibration_store_load_installation_(installation_calibration_t *out){
  if((out == nullptr) || !s_cal_storage_ready || !s_cal_prefs.isKey(KEY_INSTALL)){
    return false;
  }

  calibration_store_installation_record_t stored = {};
  const size_t n = s_cal_prefs.getBytes(KEY_INSTALL, &stored, sizeof(stored));
  if(n != sizeof(stored)){
    return false;
  }
  if(!installation_record_valid_(&stored)){
    return false;
  }

  installation_from_store_(out, &stored.installation);
  return true;
}

bool calibration_store_load_installation(installation_calibration_t *out){
  return calibration_store_load_installation_(out);
}

bool calibration_store_init(void){
  s_cal_storage_ready = s_cal_prefs.begin(CALIBRATION_PREFS_NAMESPACE, false);
  return s_cal_storage_ready;
}

bool calibration_store_load(calibration_record_t *out){
  if((out == nullptr) || !s_cal_storage_ready){
    return false;
  }

  memset(out, 0, sizeof(*out));
  out->version = (uint32_t)CALIBRATION_RECORD_VERSION;

  if(!calibration_store_load_sensor_(&out->sensor)){
    return false;
  }

  // Installation calibration is independent. Missing/invalid installation
  // calibration shall not invalidate an otherwise valid sensor calibration.
  (void)calibration_store_load_installation_(&out->installation);
  return true;
}

bool calibration_store_load_reference(calibration_record_t *out){
  if((out == nullptr) || !s_cal_storage_ready){
    return false;
  }

  memset(out, 0, sizeof(*out));
  out->version = (uint32_t)CALIBRATION_RECORD_VERSION;
  if(!calibration_store_load_sensor_key_(KEY_SENSOR_REF, &out->sensor)){
    return false;
  }
  return true;
}

bool calibration_store_save_reference(const calibration_record_t *rec){
  if((rec == nullptr) || !s_cal_storage_ready || !rec->sensor.valid){
    return false;
  }
  return calibration_store_save_sensor_key_(KEY_SENSOR_REF, &rec->sensor);
}

bool calibration_store_load_candidate(calibration_record_t *out){
  if((out == nullptr) || !s_cal_storage_ready){
    return false;
  }

  memset(out, 0, sizeof(*out));
  out->version = (uint32_t)CALIBRATION_RECORD_VERSION;
  if(!calibration_store_load_sensor_key_(KEY_SENSOR_CAND, &out->sensor)){
    return false;
  }
  return true;
}

bool calibration_store_save_candidate(const calibration_record_t *rec){
  if((rec == nullptr) || !s_cal_storage_ready || !rec->sensor.valid){
    return false;
  }
  return calibration_store_save_sensor_key_(KEY_SENSOR_CAND, &rec->sensor);
}

bool calibration_store_save_latest(const calibration_record_t *rec){
  if((rec == nullptr) || !s_cal_storage_ready || !rec->sensor.valid){
    return false;
  }

  if(!calibration_store_save_sensor_(&rec->sensor)){
    return false;
  }

  if(rec->installation.valid){
    if(!calibration_store_save_installation_(&rec->installation)){
      return false;
    }
  }

  // If this save contains only a new recorder calibration, leave any
  // existing installation calibration record untouched. The installation matrix
  // represents the physical mounting orientation and can remain valid across a
  // sensor recalibration.

  // A successful calibration write clears any previous calibration fault latch.
  (void)calibration_store_fault_set(false);
  return true;
}

bool calibration_store_fault_get(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return s_cal_prefs.getBool(KEY_FAULT, false);
}

calibration_fault_reason_t calibration_store_fault_reason_get(void){
  if(!s_cal_storage_ready){
    return CAL_FAULT_NONE;
  }
  return (calibration_fault_reason_t)s_cal_prefs.getUInt(KEY_FAULT_REASON, (uint32_t)CAL_FAULT_NONE);
}

bool calibration_store_fault_set(bool fault){
  if(!s_cal_storage_ready){
    return false;
  }
  const bool ok = (s_cal_prefs.putBool(KEY_FAULT, fault) > 0u);
  if(ok && !fault){
    (void)calibration_store_fault_reason_set(CAL_FAULT_NONE);
  }
  return ok;
}

bool calibration_store_fault_reason_set(calibration_fault_reason_t reason){
  if(!s_cal_storage_ready){
    return false;
  }
  return (s_cal_prefs.putUInt(KEY_FAULT_REASON, (uint32_t)reason) > 0u);
}

static bool calibration_store_remove_key_if_present_(const char *key){
  if((key == nullptr) || !s_cal_storage_ready){
    return false;
  }
  if(!s_cal_prefs.isKey(key)){
    return true;
  }
  return s_cal_prefs.remove(key);
}

bool calibration_store_clear_recorder(void){
  if(!s_cal_storage_ready){
    return false;
  }

  bool ok = true;
  ok = calibration_store_remove_key_if_present_(KEY_SENSOR) && ok;
  ok = calibration_store_remove_key_if_present_(KEY_SENSOR_REF) && ok;
  ok = calibration_store_remove_key_if_present_(KEY_SENSOR_CAND) && ok;
  ok = calibration_store_remove_key_if_present_(KEY_FAULT) && ok;
  ok = calibration_store_remove_key_if_present_(KEY_FAULT_REASON) && ok;
  return ok;
}

bool calibration_store_clear_installation(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return calibration_store_remove_key_if_present_(KEY_INSTALL);
}

bool calibration_store_clear(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return s_cal_prefs.clear();
}
