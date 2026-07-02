// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/button_hold_helpers.cpp
 * @brief Button sampling and hold-detection helpers for power and record controls.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/button_hold_helpers.h"

#include "config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Per-button hold-tracking state used by the state machine.
typedef struct
{
  bool last_pressed;
  bool active;
  bool fired;
  TickType_t start_tick;
} button_hold_t;

// Hold trackers are kept separate so each button can be re-initialized
// independently on state entry.
static button_hold_t s_pwr = {0};
static button_hold_t s_rec = {0};

/**
 * Ms to ticks performs the button hold helpers operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `ms`.
 * Returns: Requested numeric value.
 */
static inline TickType_t ms_to_ticks_(uint32_t ms)
{
  const TickType_t t = pdMS_TO_TICKS(ms);
  return (t == 0) ? 1 : t;
}

/**
 * Reads power pressed from the underlying hardware or cached source and
 * reports whether the value is valid.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool read_power_pressed_(void)
{
  // Match the board wiring: power button active level is HIGH.
  return (digitalRead(GPIO_POWER_BUTTON) == HIGH);
}

/**
 * Reads record pressed from the underlying hardware or cached source and
 * reports whether the value is valid.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool read_record_pressed_(void)
{
  // Record switch is wired active LOW on this hardware.
  return (digitalRead(GPIO_RECORD_SWITCH) == LOW);
}

/**
 * Initializes button init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void button_init(void)
{
  pinMode(GPIO_POWER_BUTTON, INPUT_PULLUP);
  pinMode(GPIO_RECORD_SWITCH, INPUT_PULLUP);
}

/**
 * Updates or evaluates hold init state used to qualify physical button
 * gestures without blocking the state machine.
 *
 * Inputs: `b`, `pressed_now`.
 * Returns: None.
 */
static void hold_init_(button_hold_t *b, bool pressed_now)
{
  b->last_pressed = pressed_now;
  b->active = false;
  b->fired = false;
  b->start_tick = 0;
}

/**
 * Updates or evaluates hold test state used to qualify physical button
 * gestures without blocking the state machine.
 *
 * Inputs: `b`, `pressed_now`, `now`, `delay`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool hold_test_(button_hold_t *b, bool pressed_now, TickType_t now, TickType_t delay)
{
  // Rising edge: start timing a new press.
  if ((!b->last_pressed) && pressed_now) {
    b->active = true;
    b->fired = false;
    b->start_tick = now;
  }

  // Falling edge: stop the current timing window and re-arm the one-shot.
  if (b->last_pressed && (!pressed_now)) {
    b->active = false;
    b->fired = false;
  }

  b->last_pressed = pressed_now;

  // While the press is active, emit exactly one event when the threshold is met.
  if (b->active && (!b->fired)) {
    if ((now - b->start_tick) >= delay) {
      b->fired = true;
      return true;
    }
  }
  return false;
}

/**
 * Initializes init power button state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void init_power_button(void)
{
  hold_init_(&s_pwr, read_power_pressed_());
}

/**
 * Initializes init record button state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void init_record_button(void)
{
  hold_init_(&s_rec, read_record_pressed_());
}

/**
 * Updates or evaluates test power button state used to qualify physical button
 * gestures without blocking the state machine.
 *
 * Inputs: `delay_ms`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool test_power_button(uint32_t delay_ms)
{
  const TickType_t now = xTaskGetTickCount();
  return hold_test_(&s_pwr, read_power_pressed_(), now, ms_to_ticks_(delay_ms));
}

/**
 * Updates or evaluates test record button state used to qualify physical
 * button gestures without blocking the state machine.
 *
 * Inputs: `delay_ms`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool test_record_button(uint32_t delay_ms)
{
  const TickType_t now = xTaskGetTickCount();
  return hold_test_(&s_rec, read_record_pressed_(), now, ms_to_ticks_(delay_ms));
}

/**
 * Updates or evaluates power button pressed state used to qualify physical
 * button gestures without blocking the state machine.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool power_button_pressed(void)
{
  return read_power_pressed_();
}


/**
 * Updates or evaluates record button pressed state used to qualify physical
 * button gestures without blocking the state machine.
 *
 * Inputs: None.
 * Returns: `true` when the record button is currently pressed; otherwise `false`.
 */
bool record_button_pressed(void)
{
  return read_record_pressed_();
}
