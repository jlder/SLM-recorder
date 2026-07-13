// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_report_service.h
 * @brief Human-readable calibration report generation.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "src/models/calibration_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  INSTALL_CAL_REASON_INITIAL_INSTALLATION = 0,
  INSTALL_CAL_REASON_RECORDER_REINSTALLED,
  INSTALL_CAL_REASON_FAILED_RETRY
} installation_calibration_reason_t;

typedef struct {
  calibration_save_result_t result;

  bool candidate_available;
  calibration_record_t candidate;

  bool active_before_available;
  calibration_record_t active_before;

  bool reference_before_available;
  calibration_record_t reference_before;

  bool recorder_valid_after;

  bool active_after_available;
  calibration_record_t active_after;

  bool reference_after_available;
  calibration_record_t reference_after;
} recorder_calibration_report_data_t;

typedef struct {
  bool saved;
  installation_calibration_reason_t reason;

  bool recorder_calibration_available;
  calibration_record_t recorder_calibration;

  bool candidate_available;
  installation_calibration_t candidate;

  bool installation_before_available;
  installation_calibration_t installation_before;

  bool installation_after_available;
  installation_calibration_t installation_after;
} installation_calibration_report_data_t;

/** Return the human-readable installation calibration reason label. */
const char *calibration_report_installation_reason_name(installation_calibration_reason_t reason);

/** Parse a web/API reason string into an installation calibration reason. */
bool calibration_report_parse_installation_reason(const char *text,
                                                  installation_calibration_reason_t *out_reason);

/**
 * Write one recorder calibration report to /calibration_reports on the SD card.
 *
 * Parameters:
 *   data         - report data to write.
 *   out_path     - optional destination for generated report path.
 *   out_path_len - capacity of out_path.
 *
 * Return:
 *   true when the report file was written successfully.
 */
bool calibration_report_write_recorder(const recorder_calibration_report_data_t *data,
                                       char *out_path,
                                       size_t out_path_len);

/**
 * Write one installation calibration report to /calibration_reports on the SD card.
 */
bool calibration_report_write_installation(const installation_calibration_report_data_t *data,
                                           char *out_path,
                                           size_t out_path_len);

/**
 * Write a report from the valid recorder calibration already stored in NVS.
 *
 * This support-only report is used after a firmware update when the stored
 * calibration remains valid but no calibration report exists on the SD card.
 */
bool calibration_report_write_stored_recorder(const calibration_record_t *active,
                                              const calibration_record_t *reference,
                                              bool reference_available,
                                              char *out_path,
                                              size_t out_path_len);

/**
 * Write a report from the valid installation calibration already stored in NVS.
 *
 * This support-only report is used after a firmware update when the stored
 * calibration remains valid but no calibration report exists on the SD card.
 */
bool calibration_report_write_stored_installation(const calibration_record_t *recorder_calibration,
                                                  bool recorder_calibration_available,
                                                  const installation_calibration_t *installation,
                                                  char *out_path,
                                                  size_t out_path_len);

/**
 * Return the path of the last recorder calibration report written this boot.
 */
bool calibration_report_get_last_recorder_path(char *out_path, size_t out_path_len);

/**
 * Return the path of the last installation calibration report written this boot.
 */
bool calibration_report_get_last_installation_path(char *out_path, size_t out_path_len);

#ifdef __cplusplus
}
#endif
