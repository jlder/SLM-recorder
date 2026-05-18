// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/models/system_state.h
 * @brief Recorder state model shared by state, UI, and task coordination logic.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
// Recorder high-level operational states (owned by State task).
// All operational behavior shall be expressed as explicit states.
typedef enum {
  ST_OFF = 0,
  ST_BOOT,
  ST_READY,
  ST_STARTING,
  ST_RECORDING,
  ST_STOPPING,
  ST_ERROR
} recorder_state_t;
