// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/display_driver.cpp
 * @brief Display driver initialization and access to the active Arduino_GFX instance.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/display_driver.h"
#include <Arduino.h>
#include "src/board/pin_config.h"                 // provided by Waveshare library
#include <Arduino_GFX_Library.h>
#include "config.h"

// Display controller connected through the CO5300 QSPI interface.
static Arduino_DataBus *s_bus = nullptr;
static Arduino_CO5300  *s_gfx = nullptr;
extern "C" {


/**
 * Initializes display drv init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool display_drv_init(void){
  if(s_gfx) return true;

  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, HIGH);

  s_bus = new Arduino_ESP32QSPI(
      LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3
  );

  // Arduino_GFX 1.6.0 CO5300 API includes an IPS argument after rotation.
  // Keep it explicit to avoid argument shifting on clean Library Manager installs.
  s_gfx = new Arduino_CO5300(
      s_bus, LCD_RESET, 0, false, LCD_WIDTH, LCD_HEIGHT, 22, 0, 0, 0
  );

  if(!s_gfx) return false;
  if(!s_gfx->begin()) return false;

  display_brightness_set(DISPLAY_BRIGHTNESS_ACTIVE);

  s_gfx->fillScreen(BLACK);
  return true;
}

/**
 * Returns the requested display driver get gfx information from the module
 * state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
Arduino_GFX* display_driver_get_gfx(void){
  return s_gfx;
}

/**
 * Updates display brightness set state and applies the change to the owning
 * module or hardware interface.
 *
 * Inputs: `brightness` from 0 to 255.
 * Returns: None.
 */
void display_brightness_set(uint8_t brightness){
  if(s_gfx == nullptr){
    return;
  }

  // setBrightness() is provided by the concrete CO5300 display class,
  // not by the Arduino_GFX base class.
  s_gfx->setBrightness(brightness);
}

/**
 * Enters display standby by switching the CO5300 display off and removing
 * power from the AMOLED panel supply controlled by LCD_EN.  LVGL state is not
 * changed; only the physical display output is disabled.
 *
 * Inputs: None.
 * Returns: None.
 */
void display_driver_standby_enter(void){
  if(s_gfx == nullptr){
    return;
  }

  s_gfx->displayOff();
  digitalWrite(LCD_EN, LOW);
}

/**
 * Exits display standby by restoring the AMOLED panel supply and enabling the
 * CO5300 display output again.
 *
 * Inputs: None.
 * Returns: None.
 */
void display_driver_standby_exit(void){
  digitalWrite(LCD_EN, HIGH);
  delay(20);

  if(s_gfx == nullptr){
    return;
  }

  s_gfx->displayOn();
  display_brightness_set(DISPLAY_BRIGHTNESS_ACTIVE);
}


} // extern "C"
