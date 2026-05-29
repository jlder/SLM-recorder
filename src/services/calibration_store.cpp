// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_store.cpp
 * @brief Persistent storage for the latest accelerometer and installation calibration.
 */

#include "src/services/calibration_store.h"

#include <Preferences.h>
#include <string.h>
#include "config.h"

static Preferences s_cal_prefs;
static bool s_cal_storage_ready = false;

static const char *KEY_RECORD = "record";
static const char *KEY_FAULT = "fault";

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
  calibration_store_installation_t installation;
  uint32_t checksum;
} calibration_store_record_t;


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

static void record_to_store_(calibration_store_record_t *out, const calibration_record_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->version = (uint32_t)CALIBRATION_RECORD_VERSION;

  out->sensor.valid = in->sensor.valid ? 1u : 0u;
  datetime_to_store_(&out->sensor.timestamp, &in->sensor.timestamp);
  out->sensor.gravity_mg = in->sensor.gravity_mg;
  out->sensor.gain_x = in->sensor.gain_x;
  out->sensor.gain_y = in->sensor.gain_y;
  out->sensor.gain_z = in->sensor.gain_z;
  out->sensor.offset_x_mg = in->sensor.offset_x_mg;
  out->sensor.offset_y_mg = in->sensor.offset_y_mg;
  out->sensor.offset_z_mg = in->sensor.offset_z_mg;
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    out->sensor.face[i].valid = in->sensor.face[i].valid ? 1u : 0u;
    vec_to_store_(&out->sensor.face[i].mean_mg, &in->sensor.face[i].mean_mg);
    vec_to_store_(&out->sensor.face[i].stddev_mg, &in->sensor.face[i].stddev_mg);
  }

  out->installation.valid = in->installation.valid ? 1u : 0u;
  datetime_to_store_(&out->installation.timestamp, &in->installation.timestamp);
  vec_to_store_(&out->installation.mean_mg, &in->installation.mean_mg);
  vec_to_store_(&out->installation.stddev_mg, &in->installation.stddev_mg);
  memcpy(out->installation.matrix, in->installation.matrix, sizeof(out->installation.matrix));
}

static void record_from_store_(calibration_record_t *out, const calibration_store_record_t *in){
  if((out == nullptr) || (in == nullptr)){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->version = in->version;

  out->sensor.valid = (in->sensor.valid != 0u);
  datetime_from_store_(&out->sensor.timestamp, &in->sensor.timestamp);
  out->sensor.gravity_mg = in->sensor.gravity_mg;
  out->sensor.gain_x = in->sensor.gain_x;
  out->sensor.gain_y = in->sensor.gain_y;
  out->sensor.gain_z = in->sensor.gain_z;
  out->sensor.offset_x_mg = in->sensor.offset_x_mg;
  out->sensor.offset_y_mg = in->sensor.offset_y_mg;
  out->sensor.offset_z_mg = in->sensor.offset_z_mg;
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    out->sensor.face[i].valid = (in->sensor.face[i].valid != 0u);
    vec_from_store_(&out->sensor.face[i].mean_mg, &in->sensor.face[i].mean_mg);
    vec_from_store_(&out->sensor.face[i].stddev_mg, &in->sensor.face[i].stddev_mg);
  }

  out->installation.valid = (in->installation.valid != 0u);
  datetime_from_store_(&out->installation.timestamp, &in->installation.timestamp);
  vec_from_store_(&out->installation.mean_mg, &in->installation.mean_mg);
  vec_from_store_(&out->installation.stddev_mg, &in->installation.stddev_mg);
  memcpy(out->installation.matrix, in->installation.matrix, sizeof(out->installation.matrix));
  out->checksum = in->checksum;
}

/**
 * Compute a simple additive checksum over the explicit persistent byte format,
 * excluding the checksum field. This avoids depending on calibration_record_t
 * compiler padding or C++ bool representation.
 */
static uint32_t calibration_checksum_(const calibration_store_record_t *rec){
  if(rec == nullptr){
    return 0u;
  }

  calibration_store_record_t tmp = *rec;
  tmp.checksum = 0u;

  const uint8_t *bytes = (const uint8_t *)&tmp;
  uint32_t sum = 0u;
  for(size_t i = 0u; i < sizeof(tmp); ++i){
    sum += (uint32_t)bytes[i];
  }
  return sum;
}

static bool calibration_record_structurally_valid_(const calibration_store_record_t *rec){
  if(rec == nullptr){
    return false;
  }
  if(rec->version != (uint32_t)CALIBRATION_RECORD_VERSION){
    return false;
  }
  if(rec->sensor.valid == 0u){
    return false;
  }
  return (rec->checksum == calibration_checksum_(rec));
}

bool calibration_store_init(void){
  s_cal_storage_ready = s_cal_prefs.begin(CALIBRATION_PREFS_NAMESPACE, false);
  return s_cal_storage_ready;
}

bool calibration_store_load(calibration_record_t *out){
  if((out == nullptr) || !s_cal_storage_ready){
    return false;
  }

  if(!s_cal_prefs.isKey(KEY_RECORD)){
    return false;
  }

  calibration_store_record_t stored = {};
  const size_t n = s_cal_prefs.getBytes(KEY_RECORD, &stored, sizeof(stored));
  if(n != sizeof(stored)){
    return false;
  }

  if(!calibration_record_structurally_valid_(&stored)){
    return false;
  }

  record_from_store_(out, &stored);
  return true;
}

bool calibration_store_save_latest(const calibration_record_t *rec){
  if((rec == nullptr) || !s_cal_storage_ready){
    return false;
  }

  calibration_store_record_t stored = {};
  record_to_store_(&stored, rec);
  stored.version = (uint32_t)CALIBRATION_RECORD_VERSION;
  stored.sensor.valid = 1u;
  stored.checksum = 0u;
  stored.checksum = calibration_checksum_(&stored);

  const size_t n = s_cal_prefs.putBytes(KEY_RECORD, &stored, sizeof(stored));
  if(n != sizeof(stored)){
    return false;
  }

  // A successful calibration clears any previous calibration fault latch.
  (void)calibration_store_fault_set(false);
  return true;
}

bool calibration_store_fault_get(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return s_cal_prefs.getBool(KEY_FAULT, false);
}

bool calibration_store_fault_set(bool fault){
  if(!s_cal_storage_ready){
    return false;
  }
  return (s_cal_prefs.putBool(KEY_FAULT, fault) > 0u);
}

bool calibration_store_clear(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return s_cal_prefs.clear();
}
