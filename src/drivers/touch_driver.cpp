// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/touch_driver.cpp
 * @brief Touch-controller driver wrapper for initialization and raw coordinate reads.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/touch_driver.h"
#include <Arduino.h>
#include <Wire.h>
#include <memory>
#include "src/board/pin_config.h"
#include "config.h"
#include "Arduino_DriveBus_Library.h"
#include <Arduino_DriveBus.h>


// NOTE: Do NOT construct I2C / touch objects at static init time.
// ESP32 boot and library init ordering can make early construction unsafe.
static std::shared_ptr<Arduino_IIC_DriveBus> s_iic_bus;

static void TouchInterruptThunk(void);

// Touch controller instance (created during touch_drv_init()).
static std::unique_ptr<Arduino_IIC> s_ft3168;

// Touch controller is considered usable only after a successful begin().
// This prevents other tasks (e.g. UI polling) from accessing the device while
// BOOT is still running or while begin() is in progress.
static volatile bool s_touch_ready = false;

/**
 * Touch Interrupt Thunk performs the touch driver operation represented by
 * this function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: None.
 * Returns: None.
 */
static void TouchInterruptThunk(void) {
  // Mirror prototype behavior
  if (s_touch_ready && s_ft3168) {
    s_ft3168->IIC_Interrupt_Flag = true;
  }
}

/**
 * Initializes touch drv init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool touch_drv_init(void) {
  // Create bus + device on-demand to avoid static initialization issues.
  if (!s_iic_bus) {
    s_iic_bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
  }
  if (!s_ft3168) {
    s_ft3168.reset(new Arduino_FT3x68(
        s_iic_bus, FT3168_ADDRESS, DRIVEBUS_DEFAULT_VALUE,
        TP_INT, TouchInterruptThunk));
  }

  if (!s_ft3168) return false;
  s_touch_ready = false;

  // Ensure the touch INT pin is in a defined state before
  // calling into the third-party library. After flashing, the bootloader can
  // leave GPIO configuration in a transient state; defining the pin mode here
  // makes BOOT behavior deterministic.
  pinMode(TP_INT, INPUT_PULLUP);
  delay(5);

  // Defensive: clear any pending flag before begin().
  s_ft3168->IIC_Interrupt_Flag = false;
  // On this board the touch controller can come up slightly after power rails.
  // A single short retry avoids a misleading FAIL while keeping init bounded.
  bool ok = s_ft3168->begin();
  if(!ok){
    delay(120);
    ok = s_ft3168->begin();
  }


  // Attach IRQ after successful init (avoid Arduino_IIC::begin() attaching it during construction)
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(TP_INT, TouchInterruptThunk, FALLING);
  if(ok) {
    s_touch_ready = true;
  } else {
    // Touch may still work later if the controller becomes ready; keep as WARN.
  }
  return ok;
}


/**
 * Reads touch hw get raw from the underlying hardware or cached source and
 * reports whether the value is valid.
 *
 * Inputs: `x`, `y`, `pressed`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool touch_hw_get_raw(uint16_t *x, uint16_t *y, bool *pressed) {
  if (!s_ft3168 || !x || !y || !pressed) return false;

  // Contract: if touch HW is not initialized/usable, report failure.
  // Caller (touch_service/state_task) decides how to handle invalid samples.
  if (!s_touch_ready) {
    *pressed = false;
    return false;
  }

  // Match prototype behavior: only treat as touch when the interrupt flag is set.
  if (!s_ft3168->IIC_Interrupt_Flag) {
    *pressed = false;
    return true;
  }

  // IRQ flag indicates activity; X/Y are fetched over I2C when requested.
  // Perform a bounded number of immediate attempts (mirrors accel/battery).
  // IMPORTANT: only clear the IRQ flag after a successful coordinate read so
  // a transient I2C failure does not "consume" the event.

  for(uint8_t i=0; i<TOUCH_READ_MAX_RETRIES; i++){
    const uint16_t rx = (uint16_t)s_ft3168->IIC_Read_Device_Value(
        s_ft3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    const uint16_t ry = (uint16_t)s_ft3168->IIC_Read_Device_Value(
        s_ft3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);

    // DriveBus FT3x68 returns -1 on failure, which appears here as 0xFFFF.
    if (rx != 0xFFFFu && ry != 0xFFFFu) {
      // Success: consume the pending IRQ indication.
      s_ft3168->IIC_Interrupt_Flag = false;
      *x = rx;
      *y = ry;
      *pressed = true;
      return true;
    }
  }

  // All attempts failed. Leave IRQ flag set so the next poll can try again.
  *pressed = false;
  return false;
}
