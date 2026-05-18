// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/global.h
 * @brief Shared platform constants and board-level compile-time parameters.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>

// State-machine timing
static const uint32_t CFG_BOOT_TIMEOUT_MS      = 2000u;
static const uint32_t CFG_STARTING_TIMEOUT_MS  = 1500u;
static const uint32_t CFG_CLOSING_TIMEOUT_MS   = 1500u;
static const uint32_t CFG_POWERDOWN_DELAY_MS   = 1000u;
static const uint32_t CFG_STATE_TASK_PERIOD_MS = 50u;
static const uint32_t CFG_STATE_HOUSEKEEPING_PERIOD_TICKS = 20u;

// Shared hardware configuration
static const uint32_t CFG_I2C_CLOCK_HZ = 100000u;

// Task creation parameters
static const uint32_t CFG_STATE_TASK_STACK_WORDS = 4096u;
static const uint32_t CFG_STATE_TASK_PRIO        = 15u;
static const uint32_t CFG_STATE_TASK_CORE        = 0u;

static const uint32_t CFG_SD_TASK_STACK_WORDS    = 8192u;
static const uint32_t CFG_SD_TASK_PRIO           = 14u;
static const uint32_t CFG_SD_TASK_CORE           = 1u;

static const uint32_t CFG_UI_TASK_STACK_WORDS    = 8192u;
static const uint32_t CFG_UI_TASK_PRIO           = 12u;
static const uint32_t CFG_UI_TASK_CORE           = 1u;

static const uint32_t CFG_WEB_TASK_STACK_WORDS   = 6144u;
static const uint32_t CFG_WEB_TASK_PRIO          = 10u;
static const uint32_t CFG_WEB_TASK_CORE          = 0u;

// Shared capacities
static const uint32_t CFG_RING_BUFFER_CAPACITY_ITEMS = 128u;
