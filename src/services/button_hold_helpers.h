// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/button_hold_helpers.h
 * @brief Public button hold helper API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Hold-based button helpers for state-local button semantics.
//
// Design intent:
// - The state machine calls init_*() on state entry.
// - The state machine calls test_*() in the recurring section.
// - test_power_button() and test_record_button() return true once per press when
//   the hold duration reaches the requested threshold.
// - power_button_pressed() exposes the instantaneous power-button level so the
//   state machine can qualify a record-button action with a simultaneous power
//   press for destructive actions such as clearing settings.
// - No button meaning is carried across states unless the state chooses to.

/**
 * @brief Button init.
 *
 * Inputs: None.
 * Returns: None.
 */
void button_init(void);

/**
 * @brief Init power button.
 *
 * Inputs: None.
 * Returns: None.
 */
void init_power_button(void);

/**
 * @brief Init record button.
 *
 * Inputs: None.
 * Returns: None.
 */
void init_record_button(void);

/**
 * @brief Test power button.
 *
 * Inputs: `delay_ms`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool test_power_button(uint32_t delay_ms);

/**
 * @brief Test record button.
 *
 * Inputs: `delay_ms`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool test_record_button(uint32_t delay_ms);

/**
 * @brief Power button pressed.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool power_button_pressed(void);

/**
 * @brief Record button pressed.
 *
 * Inputs: None.
 * Returns: `true` when the record button is currently pressed; otherwise `false`.
 */
bool record_button_pressed(void);
