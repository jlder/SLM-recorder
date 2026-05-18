// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/task_helpers.cpp
 * @brief Shared task initialization failure handling.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/task_helpers.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * Task create failed reboot performs the task helpers operation represented by
 * this function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `task_name`.
 * Returns: None.
 */
void task_create_failed_reboot(const char *task_name){
  Serial.print("FATAL: failed to create task: ");
  Serial.println((task_name && task_name[0]) ? task_name : "<unknown>");
  Serial.flush();
  delay(1000);
  ESP.restart();

  // If restart is unavailable or delayed, keep the failed system stopped.
  for(;;){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
