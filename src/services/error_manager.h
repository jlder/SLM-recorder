// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/error_manager.h
 * @brief Public error-code definitions and error-manager API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "src/models/system_state.h"
#include "src/models/system_message.h"

// Error ownership rule:
// - error_manager stores the single active user-visible error.
// - state_task is the only task that shall raise, clear, or update error_manager.
// - Other tasks expose local health/status through narrow APIs.
// - state_task converts those local conditions into user-visible errors.

// Error codes are chosen to match the user's recovery options as closely as possible.
// In particular for SD errors:
// - ERR_SD_NO_CARD means SD media is unavailable for use. This covers not mounted at
//   boot, card removed later, or card access failing at the low level.
// - ERR_SD_FILES_FULL means the root file-count policy prevents creating a new
//   recording. The user can recover by deleting or moving files.
// - ERR_SD_SPACE_LOW means usable free space is too low for continued recording.
//   This is used both for low-space status checks and for write-side exhaustion.
// - ERR_SD_FAULT means an unexpected SD I/O failure occurred while the media still
//   appears present. This is treated as non-recoverable by the user.
typedef enum {
  ERR_NONE = 0,
  ERR_SD_NO_CARD,
  // SD contains too many files to safely create a new recording.
  // Recoverable by deleting/moving files.
  ERR_SD_FILES_FULL,
  ERR_SD_SPACE_LOW,
  ERR_SD_FAULT,
  ERR_ACCEL_NO_RESPONSE,
  ERR_PMU_FAULT,
  ERR_TOUCH_FAULT,
  ERR_RTC_INVALID,
  ERR_RINGBUFFER_OVERFLOW,
  ERR_CALIBRATION_FAULT,
  ERR_FATAL_GENERIC
} error_code_t;

/**
 * @brief Return whether an error code belongs to the SD/storage error group.
 *
 * Inputs: `code`.
 * Returns: `true` for SD/storage error codes; otherwise `false`.
 */
bool error_manager_is_sd_error(error_code_t code);

/**
 * @brief Error manager raise.
 *
 * Inputs: `code`.
 * Returns: None.
 */
void error_manager_raise(error_code_t code);

/**
 * @brief Error manager set clearable.
 *
 * Inputs: `clearable`.
 * Returns: None.
 */
void error_manager_set_clearable(bool clearable);

/**
 * @brief Error manager can clear.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool error_manager_can_clear(void);

/**
 * @brief Error manager clear active.
 *
 * Inputs: None.
 * Returns: None.
 */
void error_manager_clear_active(void);

// Returns true when an error is currently latched.

/**
 * @brief Error manager get active.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t error_manager_get_active(void);

/**
 * @brief Error manager get display message.
 *
 * Inputs: None.
 * Returns: Message identifier selected for display.
 */
msg_id_t error_manager_get_display_message(void);
