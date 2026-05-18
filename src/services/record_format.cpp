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
  typedef char _assert_cal_size[(sizeof(record_calibration_block_t)==184) ? 1 : -1];
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
 * Builds the recording filename from registration and date/time token while
 * respecting the configured buffer size.
 *
 * Inputs: `out`, `out_sz`, `registration`, `datetime_token`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool record_filename(char *out, size_t out_sz, const char *registration, const char *datetime_token){
  if(!out||out_sz<8) return false;
  const char *reg = (registration && registration[0]) ? registration : "NOREG";
  const char *tok = (datetime_token && datetime_token[0]) ? datetime_token : "00000000_000000";
  // Prototype compatibility: SD_MMC.open() expects a leading '/'
  // and the prototype uses a lowercase '.bin' extension.
  int n = snprintf(out,out_sz,"/%s_%s.bin", reg, tok);
  return (n>0) && ((size_t)n < out_sz);
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

    out->year = cal->timestamp.year;
    out->month = cal->timestamp.month;
    out->day = cal->timestamp.day;
    out->hour = cal->timestamp.hour;
    out->minute = cal->timestamp.min;
    out->second = cal->timestamp.sec;

    out->gain_x = cal->gain_x;
    out->gain_y = cal->gain_y;
    out->gain_z = cal->gain_z;

    out->offset_x_mg = cal->offset_x_mg;
    out->offset_y_mg = cal->offset_y_mg;
    out->offset_z_mg = cal->offset_z_mg;

    for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){

      out->face_mean_mg[i][0] = cal->face[i].mean_mg.x_mg;
      out->face_mean_mg[i][1] = cal->face[i].mean_mg.y_mg;
      out->face_mean_mg[i][2] = cal->face[i].mean_mg.z_mg;

      out->face_stddev_mg[i][0] = cal->face[i].stddev_mg.x_mg;
      out->face_stddev_mg[i][1] = cal->face[i].stddev_mg.y_mg;
      out->face_stddev_mg[i][2] = cal->face[i].stddev_mg.z_mg;
    }
  }

  out->checksum = checksum_8((const uint8_t*)out, sizeof(*out)-1);
}
