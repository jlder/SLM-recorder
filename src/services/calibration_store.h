// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_store.h
 * @brief Persistent storage for the latest accelerometer calibration.
 */

#pragma once

#include <stdbool.h>
#include "src/models/calibration_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize calibration NVS storage.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if the storage namespace was opened, false otherwise.
 */
bool calibration_store_init(void);

/**
 * Load the latest valid calibration record from NVS.
 *
 * Parameters:
 *   out - destination calibration record.
 *
 * Return:
 *   true if a structurally valid calibration record was loaded, false otherwise.
 */
bool calibration_store_load(calibration_record_t *out);

/**
 * Save the latest valid calibration record to NVS.
 *
 * Parameters:
 *   rec - calibration record to save.
 *
 * Return:
 *   true if saved successfully, false otherwise.
 */
bool calibration_store_save_latest(const calibration_record_t *rec);

/**
 * Return the stored calibration-fault latch.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if a calibration plausibility fault is latched, false otherwise.
 */
bool calibration_store_fault_get(void);

/**
 * Store or clear the calibration-fault latch.
 *
 * Parameters:
 *   fault - new fault latch value.
 *
 * Return:
 *   true if written successfully, false otherwise.
 */
bool calibration_store_fault_set(bool fault);

/**
 * Clear all calibration data stored in the calibration Preferences namespace.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if the namespace was cleared successfully, false otherwise.
 */
bool calibration_store_clear(void);

#ifdef __cplusplus
}
#endif
