// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/touch_driver.cpp
 * @brief Direct FT3168 touch-controller driver.
 *
 * @details The FT3168 controller is accessed directly through Wire/I2C so the
 * firmware does not depend on Arduino_DriveBus.
 */

#include "src/drivers/touch_driver.h"

#include <Arduino.h>
#include <Wire.h>

#include "src/board/pin_config.h"
#include "src/global.h"
#include "config.h"

// FT3168 / FT3x68 register subset used by this firmware.
#define FT3168_REG_XH            0x03u
#define FT3168_REG_XL            0x04u
#define FT3168_REG_YH            0x05u
#define FT3168_REG_YL            0x06u
#define FT3168_REG_POWER_MODE    0xA5u

// DriveBus initializes FT3x68 by writing 0xA5 = 0x01, then delaying 20 ms.
// The retry delay is kept longer to tolerate Arduino IDE upload/reset startup.
#define TOUCH_INIT_POST_WRITE_DELAY_MS 20u
#define TOUCH_INIT_RETRY_DELAY_MS      150u
#define TOUCH_INIT_ATTEMPTS            3u

static volatile bool s_touch_ready = false;
static volatile bool s_touch_irq_pending = false;

/**
 * Touch interrupt thunk records that the FT3168 interrupt line has indicated
 * new touch data.
 *
 * Inputs: None.
 * Returns: None.
 */
static void IRAM_ATTR TouchInterruptThunk(void) {
  if(s_touch_ready){
    s_touch_irq_pending = true;
  }
}

/**
 * Writes one byte to an FT3168 register.
 *
 * Inputs: `reg`, `value`.
 * Returns: `true` when the I2C transaction is acknowledged.
 */
static bool ft3168_write_u8_(uint8_t reg, uint8_t value) {
  Wire.beginTransmission((uint8_t)FT3168_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

/**
 * Reads one byte from an FT3168 register.
 *
 * This intentionally matches the Arduino_DriveBus read sequence: write the
 * register address, issue a STOP, then request one byte. Some touch controllers
 * are less reliable with repeated-start reads during startup.
 *
 * Inputs: `reg`, `out`.
 * Returns: `true` when the byte is read successfully.
 */
static bool ft3168_read_u8_(uint8_t reg, uint8_t *out) {
  if(out == nullptr){
    return false;
  }

  Wire.beginTransmission((uint8_t)FT3168_ADDRESS);
  Wire.write(reg);
  if(Wire.endTransmission() != 0){
    return false;
  }

  if(Wire.requestFrom((uint8_t)FT3168_ADDRESS, (uint8_t)1u) != 1u){
    while(Wire.available()){
      (void)Wire.read();
    }
    return false;
  }

  if(!Wire.available()){
    return false;
  }

  *out = (uint8_t)Wire.read();
  return true;
}

/**
 * Performs one FT3168 initialization attempt.
 *
 * Inputs: None.
 * Returns: `true` when the controller acknowledges the DriveBus-equivalent
 *          initialization write.
 */
static bool ft3168_init_attempt_(void) {
  // Match the DriveBus FT3x68 initialization behavior used by the previous
  // implementation.  The previous code passed DRIVEBUS_DEFAULT_VALUE as reset
  // pin, so DriveBus did not toggle TP_RESET.
  if(!ft3168_write_u8_(FT3168_REG_POWER_MODE, 0x01u)){
    return false;
  }

  delay((uint32_t)TOUCH_INIT_POST_WRITE_DELAY_MS);
  return true;
}

/**
 * Initializes the FT3168 touch controller.
 *
 * Inputs: None.
 * Returns: `true` when the controller is ready for touch reads.
 */
bool touch_drv_init(void) {
  s_touch_ready = false;
  s_touch_irq_pending = false;

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(CFG_I2C_CLOCK_HZ);

  pinMode(TP_INT, INPUT_PULLUP);
  detachInterrupt(digitalPinToInterrupt(TP_INT));

  bool ok = false;
  for(uint8_t attempt = 0u; attempt < (uint8_t)TOUCH_INIT_ATTEMPTS; ++attempt){
    ok = ft3168_init_attempt_();
    if(ok){
      break;
    }
    delay((uint32_t)TOUCH_INIT_RETRY_DELAY_MS);
  }

  if(ok){
    s_touch_ready = true;
    attachInterrupt(digitalPinToInterrupt(TP_INT), TouchInterruptThunk, FALLING);
  }

  return ok;
}

/**
 * Reads the latest raw touch coordinate from the FT3168.
 *
 * Inputs: `x`, `y`, `pressed`.
 * Returns: `true` when the touch controller was read successfully or no touch
 *          is pending; `false` on invalid arguments or I2C read failure.
 */
bool touch_hw_get_raw(uint16_t *x, uint16_t *y, bool *pressed) {
  if((x == nullptr) || (y == nullptr) || (pressed == nullptr)){
    return false;
  }

  *pressed = false;

  if(!s_touch_ready){
    return false;
  }

  if(!s_touch_irq_pending){
    return true;
  }

  for(uint8_t i = 0u; i < (uint8_t)TOUCH_READ_MAX_RETRIES; ++i){
    uint8_t xh = 0u;
    uint8_t xl = 0u;
    uint8_t yh = 0u;
    uint8_t yl = 0u;

    if(ft3168_read_u8_(FT3168_REG_XH, &xh) &&
       ft3168_read_u8_(FT3168_REG_XL, &xl) &&
       ft3168_read_u8_(FT3168_REG_YH, &yh) &&
       ft3168_read_u8_(FT3168_REG_YL, &yl)){
      *x = (uint16_t)(((uint16_t)(xh & 0x0Fu) << 8) | xl);
      *y = (uint16_t)(((uint16_t)(yh & 0x0Fu) << 8) | yl);
      *pressed = true;
      s_touch_irq_pending = false;
      return true;
    }
  }

  // Leave the pending flag set so the next polling cycle can try again.
  return false;
}
