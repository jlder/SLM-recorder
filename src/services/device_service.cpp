// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/device_service.cpp
 * @brief Device-service facade over low-level drivers used during boot and runtime.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/device_service.h"

#include "src/drivers/i2c_bus.h"
#include "src/drivers/pmu_driver.h"
#include "src/drivers/rtc_driver.h"
#include "src/drivers/touch_driver.h"
#include "src/drivers/accel_driver.h"
#include "src/drivers/display_driver.h"

/**
 * Initializes i2c init state or hardware resources and prepares the module for
 * later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool i2c_init(void){
  if(!i2c_ok){
    i2c_ok = i2c_bus_drv_init();
  }
  return i2c_ok;
}

/**
 * Initializes pmu init state or hardware resources and prepares the module for
 * later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_init(void){
  if(!pmu_ok){
    pmu_ok = pmu_drv_init();
  }
  return pmu_ok;
}

/**
 * Initializes rtc init state or hardware resources and prepares the module for
 * later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_init(void){
  if(!rtc_ok){
    rtc_ok = rtc_drv_init();
  }
  return rtc_ok;
}

/**
 * Initializes touch init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool touch_init(void){
  if(!touch_ok){
    touch_ok = touch_drv_init();
  }
  return touch_ok;
}

/**
 * Initializes accel init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_init(void){
  if(!accel_ok){
    accel_ok = accel_drv_init();
  }
  return accel_ok;
}

/**
 * Initializes display init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool display_init(void){
  if(!display_ok){
    display_ok = display_drv_init();
  }
  return display_ok;
}

/**
 * USB present performs the device service operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `out_present`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool usb_present(bool* out_present){
  if((out_present == nullptr) || !pmu_ok){
    return false;
  }
  return pmu_read_usb_present(out_present);
}

/**
 * Battery percent performs the device service operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `out_percent`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool battery_percent(uint8_t* out_percent){
  if((out_percent == nullptr) || !pmu_ok){
    return false;
  }
  int16_t pct = 0;
  if(!pmu_read_battery_percent(&pct)){
    return false;
  }
  *out_percent = (uint8_t)pct;
  return true;
}

/**
 * Battery low performs the device service operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool battery_low(void){
  return pmu_ok ? pmu_battery_low() : false;
}


/**
 * Shutdown device performs the device service operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void shutdown_device(void){
  if(pmu_ok){
    pmu_shutdown();
  }
}
