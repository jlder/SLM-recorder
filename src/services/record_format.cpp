// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/record_format.cpp
 * @brief Recorder binary record formatting and filename generation helpers.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/record_format.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/**
 * Computes the 8-bit checksum used by recorder binary blocks.
 *
 * Inputs: `p`, `n`.
 * Returns: Requested numeric value.
 */
static uint8_t checksum_8(const uint8_t *p, size_t n){
  uint8_t sum = 0;
  for(size_t i=0;i<n;i++){
    sum = (uint8_t)(sum + p[i]);
  }
  return sum;
}

/**
 * Static assert record size performs the record format operation represented
 * by this function and keeps the module state consistent with recorder
 * ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
static void static_assert_record_size(void){
  typedef char _assert_size[(sizeof(record_block_t)==13) ? 1 : -1];
  typedef char _assert_status_size[(sizeof(record_status_block_t)==13) ? 1 : -1];
  typedef char _assert_cal_size[(sizeof(record_calibration_block_t)==252) ? 1 : -1];
  (void)sizeof(_assert_size);
  (void)sizeof(_assert_status_size);
  (void)sizeof(_assert_cal_size);
}

/**
 * Record format builds one acceleration data block from the corrected
 * acceleration sample and timestamp used in the recording stream.
 *
 * Inputs: `out`, `ts_ms`, `s`.
 * Returns: None.
 */
void record_format_build_block(record_block_t *out, int32_t ts_ms, const accel_sample_t *s){
  static_assert_record_size();
  memset(out,0,sizeof(*out));
  out->sync=PACKET_SYNC_BYTE;
  out->id=PACKET_TYPE_ACCEL;
  out->ts_ms=ts_ms;
  out->ax=s->ax;
  out->ay=s->ay;
  out->az=s->az;
  out->checksum = checksum_8((const uint8_t*)out, sizeof(*out)-1);
}

/**
 * Builds the daily recording filename prefix from registration and the date
 * portion of the compact recording-start timestamp.
 *
 * The SD storage layer later appends _N.bin where N is the number of recording
 * sessions started in the daily file.  Only the YYYYMMDD part of the token is
 * used so all recordings of one day are appended to one file.
 *
 * Inputs: `out`, `out_sz`, `registration`, `datetime_token`.
 * Returns: `true` when the prefix was built; otherwise `false`.
 */
bool record_daily_prefix(char *out, size_t out_sz, const char *registration, const char *datetime_token){
  if((out == nullptr) || (out_sz < 16u)){
    return false;
  }

  const char *reg = (registration && registration[0]) ? registration : "NOREG";
  const char *tok = (datetime_token && datetime_token[0]) ? datetime_token : "00000000_000000";

  // timebase_get_datetime_compact() provides YYYYMMDD_HHMMSS.  Daily files
  // intentionally keep only the date so several start/stop recording sessions
  // on the same day are stored in the same SD file.
  char date[9] = "00000000";
  if(strlen(tok) >= 8u){
    memcpy(date, tok, 8u);
    date[8] = '\0';
  }

  const int n = snprintf(out, out_sz, "/%s_%s", reg, date);
  return (n > 0) && ((size_t)n < out_sz);
}


/**
 * Record format builds the final status block containing the saturated ring-
 * buffer overflow count written during recording close.
 *
 * Inputs: `out`, `overflow_count`.
 * Returns: None.
 */
void record_format_build_status_block(record_status_block_t *out, uint32_t overflow_count){
  static_assert_record_size();
  memset(out,0,sizeof(*out));
  out->sync=PACKET_SYNC_BYTE;
  out->id=PACKET_TYPE_STATUS;
  if(overflow_count > 0xFFFFu){
    overflow_count = 0xFFFFu;
  }
  out->overflow = (uint16_t)overflow_count;
  out->checksum = checksum_8((const uint8_t*)out, sizeof(*out)-1);
}


/**
 * Record format builds the calibration block written at the start of a
 * recording file, copying the active calibration coefficients and six-face
 * capture data.
 *
 * Inputs: `out`, `cal`.
 * Returns: None.
 */
void record_format_build_calibration_block(record_calibration_block_t *out,
                                           const calibration_record_t *cal){
  static_assert_record_size();

  if(out == nullptr){
    return;
  }

  memset(out, 0, sizeof(*out));
  out->sync = PACKET_SYNC_BYTE;
  out->id = PACKET_TYPE_CALIBRATION;
  out->size = (uint16_t)sizeof(*out);

  if(cal != nullptr){
    out->calibration_version = cal->version;

    out->year = cal->sensor.timestamp.year;
    out->month = cal->sensor.timestamp.month;
    out->day = cal->sensor.timestamp.day;
    out->hour = cal->sensor.timestamp.hour;
    out->minute = cal->sensor.timestamp.min;
    out->second = cal->sensor.timestamp.sec;

    out->gain_x = cal->sensor.gain_x;
    out->gain_y = cal->sensor.gain_y;
    out->gain_z = cal->sensor.gain_z;

    out->offset_x_mg = cal->sensor.offset_x_mg;
    out->offset_y_mg = cal->sensor.offset_y_mg;
    out->offset_z_mg = cal->sensor.offset_z_mg;

    for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){

      out->face_mean_mg[i][0] = cal->sensor.face[i].mean_mg.x_mg;
      out->face_mean_mg[i][1] = cal->sensor.face[i].mean_mg.y_mg;
      out->face_mean_mg[i][2] = cal->sensor.face[i].mean_mg.z_mg;

      out->face_stddev_mg[i][0] = cal->sensor.face[i].stddev_mg.x_mg;
      out->face_stddev_mg[i][1] = cal->sensor.face[i].stddev_mg.y_mg;
      out->face_stddev_mg[i][2] = cal->sensor.face[i].stddev_mg.z_mg;
    }

    out->installation_valid = cal->installation.valid ? 1u : 0u;
    out->installation_year = cal->installation.timestamp.year;
    out->installation_month = cal->installation.timestamp.month;
    out->installation_day = cal->installation.timestamp.day;
    out->installation_hour = cal->installation.timestamp.hour;
    out->installation_minute = cal->installation.timestamp.min;
    out->installation_second = cal->installation.timestamp.sec;
    out->installation_mean_mg[0] = cal->installation.mean_mg.x_mg;
    out->installation_mean_mg[1] = cal->installation.mean_mg.y_mg;
    out->installation_mean_mg[2] = cal->installation.mean_mg.z_mg;
    out->installation_stddev_mg[0] = cal->installation.stddev_mg.x_mg;
    out->installation_stddev_mg[1] = cal->installation.stddev_mg.y_mg;
    out->installation_stddev_mg[2] = cal->installation.stddev_mg.z_mg;
    for(uint32_t i = 0u; i < 9u; ++i){
      out->installation_matrix[i] = cal->installation.matrix[i];
    }

  }

  out->checksum = checksum_8((const uint8_t*)out, sizeof(*out)-1);
}
