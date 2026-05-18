/**
 * @file recorder_baseline_v2000.ino
 * @brief Arduino entry point that initializes board services and keeps the main loop idle while FreeRTOS tasks run.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include <Arduino.h>
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "src/tasks/sd_task.h"
#include "src/tasks/state_task.h"
#include "src/tasks/web_task.h"
#include "src/tasks/ui_task.h"
#include "src/services/device_service.h"
#include "src/services/touch_service.h"
#include "src/services/settings_store.h"

bool i2c_ok = false;
bool pmu_ok = false;
bool rtc_ok = false;
bool touch_ok = false;
bool accel_ok = false;
bool display_ok = false;
bool settings_storage_ok = false;

/**
 * @brief Initialize the Arduino runtime and start recorder tasks.
 *
 * Inputs: None.
 * Returns: None.
 */
void setup() {
  // Start serial output for boot diagnostics.
  Serial.begin(115200);

  // Give the USB serial bridge a brief settle time.
  delay(50);

  // Initialize the shared I2C bus through the abstraction layer.
  i2c_ok = i2c_init();

  // Perform a first PMU initialization attempt through the abstraction layer.
  pmu_ok = pmu_init();

  // Perform a first RTC initialization attempt through the abstraction layer.
  rtc_ok = rtc_init();

  // Perform a first touch-controller initialization attempt through the abstraction layer.
  touch_ok = touch_init();

  // Keep touch inactive until the state machine enables it in READY.
  touch_enable(false);

  // Perform a first accelerometer initialization attempt through the abstraction layer.
  accel_ok = accel_init();

  // Initialize the display before the UI task starts.
  display_ok = display_init();

  // Start the UI task early so boot progress can be shown.
  if(display_ok) {
    ui_task_init();
  }

  // Open the settings storage backend before entering the state loop.
  settings_storage_ok = settings_init();

  // Start the state task after the first deterministic init pass.
  state_task_init();

  // Start the web task after the state task.
  web_task_init();

  // Start the SD task last because it is the most timing/GPIO-sensitive subsystem.
  sd_task_init();
}

/**
 * @brief Keep the Arduino loop idle while FreeRTOS tasks execute.
 *
 * Inputs: None.
 * Returns: None.
 */
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
