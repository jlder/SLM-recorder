// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_store.h
 * @brief Persistent storage for recorder calibration history and installation calibration.
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

bool calibration_store_load_reference(calibration_record_t *out);
bool calibration_store_save_reference(const calibration_record_t *rec);
bool calibration_store_load_candidate(calibration_record_t *out);
bool calibration_store_save_candidate(const calibration_record_t *rec);
bool calibration_store_load_installation(installation_calibration_t *out);

/**
 * Return the stored calibration-fault latch.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if a calibration fault is latched, false otherwise.
 */
bool calibration_store_fault_get(void);
calibration_fault_reason_t calibration_store_fault_reason_get(void);

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
bool calibration_store_fault_reason_set(calibration_fault_reason_t reason);

/**
 * Clear all calibration data stored in the calibration Preferences namespace.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if the namespace was cleared successfully, false otherwise.
 */

/**
 * Clear recorder calibration history and recorder calibration fault state while
 * preserving the stored installation calibration. This is a support-only action.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if the recorder calibration keys were removed successfully, false otherwise.
 */
bool calibration_store_clear_recorder(void);

/**
 * Clear only the stored installation calibration. This is a support-only action.
 * Recorder calibration history and fault state are preserved.
 */
bool calibration_store_clear_installation(void);

#ifdef __cplusplus
}
#endif
