// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/sd_task.h
 * @brief Public SD task lifecycle, recorder command, and SD error-status API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "src/services/error_manager.h"

// SD state machine owned by sd_task.
typedef enum {
  SD_BOOT = 0,
  SD_IDLE,
  SD_OPEN,
  SD_WRITING,
  SD_CLOSING,
  SD_ERROR
} sd_state_t;

/**
 * @brief SD task init.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_task_init(void);
/**
 * @brief SD request open.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_request_open(void);
/**
 * @brief SD request close.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_request_close(void);
/**
 * @brief SD request ack error.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_request_ack_error(void);

// SD error reporting role:
// - sd_task owns SD fault classification and SD recovery probing.
// - sd_task does not raise error_manager errors directly.
// - state_task polls this API and publishes user-visible errors.
// Semantic SD status and error interface.
/**
 * @brief SD error get.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_error_get(void);
/**
 * @brief SD error show ok clear.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_error_show_ok_clear(void);
/**
 * @brief SD is open.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_is_open(void);
/**
 * @brief SD is closed.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_is_closed(void);
