// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/web_task.h
 * @brief Public web task lifecycle and enable-state API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>

/**
 * @brief Web task init.
 *
 * Inputs: None.
 * Returns: None.
 */
void web_task_init(void);
/**
 * @brief Web task set enabled.
 *
 * Inputs: `enabled`.
 * Returns: None.
 */
void web_task_set_enabled(bool enabled);
/**
 * @brief Web task is enabled.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool web_task_is_enabled(void);

