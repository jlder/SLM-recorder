// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/state_task.h
 * @brief Public state task lifecycle, command, and status API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include "src/models/system_status.h"

/**
 * @brief Initialize and start the State task.
 *
 * Creates the State task. Must be called once during system setup.
 *
 * @timing O(1) enqueue/copy operations; does not block on I/O.
 * @safety IMPORTANT
 */
void state_task_init(void);

/**
 * @brief Post an event to the State task.
 *
 * Latches an asynchronous event for the State task to process. Returns false if inputs are invalid.
 *
 * @timing O(1) enqueue/copy operations; does not block on I/O.
 * @safety IMPORTANT
 */
/**
 * @brief Get a coherent snapshot of the current system status.
 *
 * Returns the latest published system_status snapshot. Snapshot is a simple last-published copy (single-writer, multi-reader).
 *
 * @timing O(1) enqueue/copy operations; does not block on I/O.
 * @safety NORMAL
 */
system_status_t state_task_get_status(void);


// NOTE: Persistence of settings to non-volatile storage is owned by the
// settings_store service. The State task only *reads* settings to gate
// transitions (e.g., registration configured before recording).

