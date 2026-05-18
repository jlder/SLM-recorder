// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/i2c_bus.cpp
 * @brief I2C bus recovery and initialization for shared board peripherals.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/i2c_bus.h"

#include <Arduino.h>
#include <Wire.h>
#include "src/board/pin_config.h"

#include "src/global.h"  // CFG_I2C_CLOCK_HZ

static bool s_i2c_inited = false;

// Attempt to recover a stuck I2C bus (e.g., a peripheral holding SDA low)
// by clocking SCL and issuing a STOP condition.
//
// Rationale:
// - After a firmware upload/reset, some peripherals may remain powered and keep
//   internal state that can leave the bus in a non-idle condition.
// - Recovering the bus here keeps ownership centralized and makes BOOT
//   prerequisites more deterministic.
//
// The system SHALL attempt an I2C bus recovery before initializing
//             Wire to reduce the risk of peripherals failing to respond at BOOT.
/**
 * I2C bus recover performs the i2c bus operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
static void i2c_bus_recover_(void) {
  // Configure lines as GPIO for recovery attempt.
  pinMode(IIC_SCL, OUTPUT);
  pinMode(IIC_SDA, INPUT_PULLUP);
  digitalWrite(IIC_SCL, HIGH);
  delayMicroseconds(5);

  // If SDA is low, try to clock it free.
  if (digitalRead(IIC_SDA) == LOW) {
    for (int i = 0; i < 9; i++) {
      digitalWrite(IIC_SCL, LOW);
      delayMicroseconds(5);
      digitalWrite(IIC_SCL, HIGH);
      delayMicroseconds(5);
      if (digitalRead(IIC_SDA) == HIGH) {
        break;
      }
    }

    // Issue a STOP condition: SDA low -> SCL high -> SDA high.
    pinMode(IIC_SDA, OUTPUT);
    digitalWrite(IIC_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(IIC_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(IIC_SDA, HIGH);
    delayMicroseconds(5);
    pinMode(IIC_SDA, INPUT_PULLUP);
  }
}

/**
 * Initializes i2c bus drv init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool i2c_bus_drv_init(void) {
  if (s_i2c_inited) {
    return true;
  }

  // Apply pins and frequency before any peripheral init.
  // Note: Wire.begin() must occur before Wire.setClock() on ESP32.
  i2c_bus_recover_();
  (void)Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(CFG_I2C_CLOCK_HZ);

  s_i2c_inited = true;
  return true;
}
