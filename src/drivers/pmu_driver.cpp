// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/pmu_driver.cpp
 * @brief Power-management driver wrapper for battery, USB, and shutdown functions.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/pmu_driver.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include "src/board/pin_config.h"
#include "config.h"
#include <XPowersLib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static XPowersPMU s_pmu;

/**
 * Disable AXP2101 Power Key Shutdown performs the pmu driver operation
 * represented by this function and keeps the module state consistent with
 * recorder ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
static void disableAXP2101PowerKeyShutdown(void) {
  // Copied from prototype: attempt to disable long-press hardware shutdown.
  // The register write and messaging are kept as in the working code.
  uint8_t value = s_pmu.readRegister(0x22);
  value &= ~(1 << 2);
  if (s_pmu.writeRegister(0x22, value)) {
  } else {
    // Observed on some boards: write may report failure, while the setting can
    // still take effect. Keep this as DEBUG to avoid confusing operators.
  }
}

/**
 * Initializes pmu drv init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_drv_init(void) {
  // Prototype: PMU.begin(Wire, AXP2101_ADDRESS, IIC_SDA, IIC_SCL);
  if (!s_pmu.begin(Wire, AXP2101_ADDRESS, IIC_SDA, IIC_SCL)) {
    return false;
  }
  delay(50);
  disableAXP2101PowerKeyShutdown();
  s_pmu.enableVbusVoltageMeasure();
  return true;
}

/**
 * Reads pmu battery percent from the underlying hardware or cached source and
 * reports whether the value is valid.
 *
 * Inputs: `out_percent`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_read_battery_percent(int16_t* out_percent) {
  if(out_percent == nullptr) return false;

  // Mirror accelerometer contract: bounded retry in the driver.
  for(uint8_t i = 0; i < (uint8_t)PMU_BATT_PCT_MAX_RETRIES; i++){
    const int v = s_pmu.getBatteryPercent();
    if((v >= 0) && (v <= 100)){
      *out_percent = (int16_t)v;
      return true;
    }
    // No delay: bounded loop, keep worst-case time deterministic.
  }
  return false;
}

/**
 * Reads pmu usb present from the underlying hardware or cached source and
 * reports whether the value is valid.
 *
 * Inputs: `out_present`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_read_usb_present(bool* out_present){
  if(out_present == nullptr) return false;

  // XPowersLib does not expose an error code for isVbusIn().
  // We treat a consistent sample across bounded retries as "valid".
  bool first = false;
  bool have_first = false;
  uint8_t same = 0u;

  for(uint8_t i = 0; i < (uint8_t)PMU_USB_MAX_RETRIES; i++){
    const bool v = s_pmu.isVbusIn();
    if(!have_first){
      first = v;
      have_first = true;
      same = 1u;
    } else {
      if(v == first){
        same++;
      } else {
        // Inconsistent readings within the retry window.
        return false;
      }
    }
  }

  if(have_first && (same >= (uint8_t)PMU_USB_MAX_RETRIES)){
    *out_present = first;
    return true;
  }
  return false;
}

/**
 * PMU is USB connected performs the pmu driver operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_is_usb_connected(void) {
  return s_pmu.isVbusIn();
}


/**
 * PMU battery low performs the pmu driver operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_battery_low(void) {
  int16_t pct = 0;
  if(!pmu_read_battery_percent(&pct)){
    // Invalid sample: caller decides how to handle error.
    return false;
  }
  return ((uint16_t)pct <= (uint16_t)PMU_BATT_LOW_THRESHOLD_PCT) && !pmu_is_usb_connected();
}


/**
 * PMU shutdown performs the pmu driver operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void pmu_shutdown(void){
  // Try to shut down through PMU. If it fails, reboot as fallback.
  s_pmu.shutdown();
vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
}

