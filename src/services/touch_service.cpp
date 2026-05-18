// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/touch_service.cpp
 * @brief Touch service that gates raw touch reads and exposes a cached touch snapshot.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/touch_service.h"

#include "src/drivers/touch_driver.h"
#include "src/services/device_service.h"

static touch_snapshot_t s_snap;
static bool s_touch_enabled = false;

/**
 * Touch enable performs the touch service operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `enabled`.
 * Returns: None.
 */
void touch_enable(bool enabled){
  s_touch_enabled = (enabled && touch_ok);
}

/**
 * Report whether touch sampling is enabled performs the touch service
 * operation represented by this function and keeps the module state consistent
 * with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when touch sampling is enabled; otherwise `false`.
 */
bool touch_is_enabled(void){
  return s_touch_enabled;
}

/**
 * Touch service update from hw performs the touch service operation
 * represented by this function and keeps the module state consistent with
 * recorder ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void touch_service_update_from_hw(void){
  if(!s_touch_enabled) return;

  uint16_t x = 0;
  uint16_t y = 0;
  bool pressed = false;
  if(touch_hw_get_raw(&x, &y, &pressed)){
    s_snap.x = x;
    s_snap.y = y;
    s_snap.pressed = pressed;
    s_snap.valid = true;
  } else {
    s_snap.valid = false;
    s_snap.pressed = false;
  }
}

/**
 * Returns the requested touch service get snapshot information from the module
 * state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Current touch snapshot.
 */
touch_snapshot_t touch_service_get_snapshot(void){
  return s_snap;
}
