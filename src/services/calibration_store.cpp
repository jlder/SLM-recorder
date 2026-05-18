// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_store.cpp
 * @brief Persistent storage for the latest accelerometer calibration.
 */

#include "src/services/calibration_store.h"

#include <Preferences.h>
#include <string.h>
#include "config.h"

static Preferences s_cal_prefs;
static bool s_cal_storage_ready = false;

static const char *KEY_RECORD = "record";
static const char *KEY_FAULT = "fault";

/**
 * Compute a simple checksum over the calibration record, excluding the checksum field.
 *
 * Parameters:
 *   rec - calibration record to checksum.
 *
 * Return:
 *   additive byte checksum.
 */
static uint32_t calibration_checksum_(const calibration_record_t *rec){
  if(rec == nullptr){
    return 0u;
  }

  calibration_record_t tmp = *rec;
  tmp.checksum = 0u;

  const uint8_t *bytes = (const uint8_t *)&tmp;
  uint32_t sum = 0u;
  for(size_t i = 0u; i < sizeof(tmp); ++i){
    sum += (uint32_t)bytes[i];
  }
  return sum;
}

/**
 * Check whether a loaded calibration record is structurally valid.
 *
 * Parameters:
 *   rec - calibration record to check.
 *
 * Return:
 *   true if the record has the expected version, valid marker, and checksum.
 */
static bool calibration_record_structurally_valid_(const calibration_record_t *rec){
  if(rec == nullptr){
    return false;
  }
  if(rec->version != (uint32_t)CALIBRATION_RECORD_VERSION){
    return false;
  }
  if(!rec->valid){
    return false;
  }
  return (rec->checksum == calibration_checksum_(rec));
}

/**
 * Initializes the calibration Preferences namespace used to store the latest
 * calibration record and fault latch.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_store_init(void){
  s_cal_storage_ready = s_cal_prefs.begin(CALIBRATION_PREFS_NAMESPACE, false);
  return s_cal_storage_ready;
}

/**
 * Loads the latest stored calibration record from NVS and verifies its version
 * and checksum.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_store_load(calibration_record_t *out){
  if((out == nullptr) || !s_cal_storage_ready){
    return false;
  }

  if(!s_cal_prefs.isKey(KEY_RECORD)){
    return false;
  }

  calibration_record_t rec = {};
  const size_t n = s_cal_prefs.getBytes(KEY_RECORD, &rec, sizeof(rec));
  if(n != sizeof(rec)){
    return false;
  }

  if(!calibration_record_structurally_valid_(&rec)){
    return false;
  }

  *out = rec;
  return true;
}

/**
 * Handles calibration store save latest for calibration storage, status,
 * session processing, or driver correction state.
 *
 * Inputs: `rec`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_store_save_latest(const calibration_record_t *rec){
  if((rec == nullptr) || !s_cal_storage_ready){
    return false;
  }

  calibration_record_t tmp = *rec;
  tmp.version = (uint32_t)CALIBRATION_RECORD_VERSION;
  tmp.valid = true;
  tmp.checksum = 0u;
  tmp.checksum = calibration_checksum_(&tmp);

  const size_t n = s_cal_prefs.putBytes(KEY_RECORD, &tmp, sizeof(tmp));
  if(n != sizeof(tmp)){
    return false;
  }

  // A successful calibration clears any previous calibration fault latch.
  // Do not fail the calibration save only because the fault-latch clear
  // operation reports no write/update; the calibration record is the primary
  // stored artifact.
  (void)calibration_store_fault_set(false);
  return true;
}

/**
 * Reads the stored calibration fault latch from NVS.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_store_fault_get(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return s_cal_prefs.getBool(KEY_FAULT, false);
}

/**
 * Writes the calibration fault latch to NVS.
 *
 * Inputs: `fault`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_store_fault_set(bool fault){
  if(!s_cal_storage_ready){
    return false;
  }
  return (s_cal_prefs.putBool(KEY_FAULT, fault) > 0u);
}


/**
 * Clears all stored calibration keys from the calibration Preferences
 * namespace.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_store_clear(void){
  if(!s_cal_storage_ready){
    return false;
  }
  return s_cal_prefs.clear();
}
